#include "traceroute.h"
#include <unistd.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdbool.h>
#include <asm-generic/errno.h>
#include <errno.h>
#include <netinet/ip_icmp.h>
#include <sys/time.h>
#include "traceroute.h"
#include <arpa/inet.h>
#include <linux/errqueue.h>

/*
 * Prototypes
 *
 */

static void trcrt_send();
static void trcrt_receive();

static void trcrt_handle_icmp();
static void trcrt_handle_tcp();
static void trcrt_handle_udp();
static void trcrt_print_result();

static __suseconds_t time_diff(struct timeval* begin, struct timeval *end);

int send_icmp_msg_v4(int sock, uint16_t id, uint8_t ttl, uint8_t icmp_type,
        uint16_t icmp_seq_num, size_t payload_size, in_addr_t source_ip,
        in_addr_t dest_ip, uint8_t tos);
int send_udp_trcrt_msg(int udp_sock, uint8_t ttl, size_t payload_size,
        in_addr_t dest_ip, in_port_t dest_port, uint8_t tos);
int get_name_by_ipaddr(in_addr_t ip, char *host,
        size_t host_len, bool *in_cache);

/*
 * Main logic
 *
 */

void process_trace() {
    for (
            int i = 1;
            g_tcrt_ctx.current_ttl < g_tcrt_ctx.max_ttl;
            g_tcrt_ctx.current_ttl++, i++) {

        printf("%2d  ", i);
        for (uint8_t j = 0; j < g_tcrt_ctx.query_count; j++, g_tcrt_ctx.dest_port++) {
            // Pipeline
            trcrt_send();
            trcrt_receive();
            trcrt_print_result();

            sleep(g_tcrt_ctx.send_wait);
        }
        printf("\n");

        if (g_tcrt_ctx.end_tracing) break;
    }
}

static void trcrt_send() {
    int ret = 0;

    if (g_tcrt_ctx.flags & TRCRT_ICMP) {
        ret = send_icmp_msg_v4(
                g_tcrt_ctx.sock,
                getpid(),
                g_tcrt_ctx.current_ttl,
                ICMP_ECHO,
                g_tcrt_ctx.dest_port /* according to man */,
                30,
                g_tcrt_ctx.source_ip,
                g_tcrt_ctx.dest_ip,
                g_tcrt_ctx.tos);
    }
    else if (g_tcrt_ctx.flags & TRCRT_TCP) {
        ;
    }
    else {
        g_tcrt_ctx.sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
        ret = send_udp_trcrt_msg(
                g_tcrt_ctx.sock,
                g_tcrt_ctx.current_ttl,
                32, // TODO FIX
                g_tcrt_ctx.dest_ip,
                g_tcrt_ctx.dest_port,
                g_tcrt_ctx.tos);
    }

    if (ret != 0) {
        perror("cannot send msg");
        exit(EXIT_FAILURE);
    }

    ret = gettimeofday(&g_tcrt_ctx.time_sent, NULL);
    if (ret != 0) {
        perror("cannot set time");
        exit(EXIT_FAILURE);
    }
}

static void trcrt_receive() {
    alarm(DEFAULT_RESP_TIME_WAIT);

    if (g_tcrt_ctx.flags & TRCRT_ICMP) {
        trcrt_handle_icmp();
    }
    else if (g_tcrt_ctx.flags & TRCRT_TCP) {
        trcrt_handle_tcp();
    }
    else {
        trcrt_handle_udp();
    }
}

static void trcrt_handle_icmp() {
    ssize_t ret;
    struct icmphdr* icmp;
    struct iphdr* ip;
    char   buffer[2048];

    ret = recvfrom(g_tcrt_ctx.sock, buffer, sizeof buffer, 0, NULL, 0);

    if (ret > 0) {
        ip = (struct iphdr *)buffer;
        icmp = (struct icmphdr*)(buffer + sizeof (struct iphdr));
        g_tcrt_ctx.answer_ip = ip->saddr;

        switch (icmp->type)
        {
        case ICMP_ECHOREPLY:
            g_tcrt_ctx.rc = HAVEANSWER;
            g_tcrt_ctx.end_tracing = 1;
            break;
        case ICMP_TIME_EXCEEDED:
            g_tcrt_ctx.rc = TTLEXCEEDED;
            break;
        default:
            g_tcrt_ctx.rc = UNKNOWN;
            break;
        }
        return;
    }
    if (ret == 0) {
        fprintf(stderr, "Connection closed\n");
        exit(EXIT_FAILURE);
    }
    if (errno == EINTR) {
        g_tcrt_ctx.rc = NOANSWER;
        printf("no answer\n");
        return;
    }

    perror("cannot receive message");
    exit(EXIT_FAILURE);
}

static void trcrt_handle_udp() {

    char   buffer[2048];
    struct iovec   iov[1];
    struct msghdr  msg;
    ssize_t ret = 0;

    // Init message struct
    memset(&msg, 0, sizeof(msg));
    memset(iov, 0, sizeof(iov));
    iov[0].iov_base = buffer;
    iov[0].iov_len = sizeof buffer;
    msg.msg_iov     = iov;
    msg.msg_iovlen  = 1;

    char    buffer2[1024];
    struct cmsghdr *cmsg;
    struct sock_extended_err *sock_err;
    struct sockaddr_in remote;

    msg.msg_name = (void *)&remote;
    msg.msg_namelen = sizeof(remote);

    msg.msg_flags = 0;

    msg.msg_control = buffer2;
    msg.msg_controllen = sizeof(buffer2);

    memset(&remote, 0, sizeof remote);

    g_tcrt_ctx.try_read = 1;
    while (g_tcrt_ctx.try_read) {
        ret = recvmsg(g_tcrt_ctx.sock, &msg, MSG_ERRQUEUE);

        if (ret < 0) continue; // Have not come yet, try again...

        cmsg = CMSG_FIRSTHDR(&msg);

        if (cmsg->cmsg_level == SOL_IP
            && cmsg->cmsg_type == IP_RECVERR) {
            sock_err = (struct sock_extended_err *)CMSG_DATA(cmsg);

            switch (sock_err->ee_type)
            {
            case ICMP_NET_UNREACH:
                fprintf(stderr, "Network Unreachable Error\n");
                break;
            case ICMP_HOST_UNREACH:
                fprintf(stderr, "Host Unreachable Error\n");
                break;
            case ICMP_NET_UNR_TOS:
                g_tcrt_ctx.rc = TTLEXCEEDED;
                break;
            case ICMP_UNREACH_PORT:
                g_tcrt_ctx.rc = HAVEANSWER;
                g_tcrt_ctx.end_tracing = 1;
                break;
            }
            /* Handle all other cases. Find more errors :
             * http://lxr.linux.no/linux+v3.5/include/linux/icmp.h#L39
             */

            // Get sender's IP
            g_tcrt_ctx.answer_ip = ((struct sockaddr_in*)SO_EE_OFFENDER(sock_err))->sin_addr.s_addr;
        }
        else {
            printf("unknown\n");
        }
        break;
    }

    if (!g_tcrt_ctx.try_read) {
        g_tcrt_ctx.rc = NOANSWER;
    }
}

static void trcrt_handle_tcp() {

}

static void trcrt_print_result() {
    char hostname[NI_MAXHOST] = { 0 };
    struct timeval msg_rcv_time;
    bool in_cache;
    int ret;

    ret = gettimeofday(&msg_rcv_time, NULL);
    if (ret != 0) {
        perror("cannot set time of receiving");
        exit(EXIT_FAILURE);
    }

    if (get_name_by_ipaddr(g_tcrt_ctx.answer_ip, hostname, sizeof hostname, &in_cache) != 0) {
        fprintf(stderr, "error\n");
        exit(EXIT_FAILURE);
    }
    __suseconds_t rtt = time_diff(&g_tcrt_ctx.time_sent, &msg_rcv_time);

    if (!in_cache) {
        printf("%s ", hostname);
        printf("(%s)  ", inet_ntoa((struct in_addr){g_tcrt_ctx.answer_ip}));
    }

    switch (g_tcrt_ctx.rc)
    {
    case HAVEANSWER:
    case TTLEXCEEDED:
        printf("%ld.%03ld ms  ", rtt / 1000, rtt % 1000);
        break;
    default:
        printf("*  ");
        break;
    }
    fflush(stdout);
}

static __suseconds_t time_diff(struct timeval* begin, struct timeval *end) {
    __suseconds_t ret;

    ret = end->tv_sec - begin->tv_sec;
    ret *= MICROSECONDS_IN_SECOND;
    ret += (end->tv_usec - begin->tv_usec);

    return ret;
}