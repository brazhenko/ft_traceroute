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
static void trcrt_handle_icmp(struct msghdr *msg);
static void trcrt_handle_udp(struct msghdr *msg);
static void trcrt_print_result();


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

    for (int i = 1; g_tcrt_ctx.current_ttl < g_tcrt_ctx.max_ttl; g_tcrt_ctx.current_ttl++, i++) {
        printf("%2d  ", i);
        for (int j = 0; j < DEFAULT_PROBES_ONE_TTL; j++) {
            trcrt_send();
            trcrt_receive();
            trcrt_print_result();

            sleep(g_tcrt_ctx.send_wait);
            g_tcrt_ctx.dest_port++;
        }
        printf("\n");

        if (g_tcrt_ctx.rc == HAVEANSWER) break;
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
                g_tcrt_ctx.source_ip, // TODO fix for source
                g_tcrt_ctx.dest_ip,
                g_tcrt_ctx.tos);
    }
    else if (g_tcrt_ctx.flags & TRCRT_TCP)
        ;
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

    alarm(3);

    if (g_tcrt_ctx.flags & TRCRT_ICMP) {
        trcrt_handle_icmp(&msg);
    }
    else if (g_tcrt_ctx.flags & TRCRT_TCP)
        ;
    else
        trcrt_handle_udp(&msg);

    if (ret < 0) {
        perror("cannot create socket");
        exit(EXIT_FAILURE);
    }
}

static void trcrt_handle_icmp(struct msghdr *msg) {
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
            break;
        case ICMP_TIME_EXCEEDED:
            g_tcrt_ctx.rc = TTLEXCEEDED;
            break;
        default:
            g_tcrt_ctx.rc = UNKNOWN;
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

static void trcrt_handle_udp(struct msghdr *msg) {
    char    buffer2[1024];
    struct cmsghdr *cmsg;
    struct sock_extended_err *sock_err;
    ssize_t ret;
    struct sockaddr_in remote;

    char   buffer[2048];
    struct iovec   iov[1];
    // Init message struct

    iov[0].iov_base = buffer;
    iov[0].iov_len = sizeof buffer;
    msg->msg_iov     = iov;
    msg->msg_iovlen  = 1;

    msg->msg_name = (void *)&remote;
    msg->msg_namelen = sizeof(remote);

    msg->msg_flags = 0;
    msg->msg_control = buffer2;
    msg->msg_controllen = sizeof(buffer2);

//    ret = setsockopt(g_tcrt_ctx.sock, IPPROTO_IP, IP_PKTINFO, (int[1]){ 1 }, sizeof(int));
//    if (ret != 0) {
//        perror("shit");
//        exit(EXIT_FAILURE);
//    }
    memset(&remote, 0, sizeof remote);

    g_tcrt_ctx.try_read = 1;
    while (g_tcrt_ctx.try_read) {
        ret = recvmsg(g_tcrt_ctx.sock, msg, MSG_ERRQUEUE);

        if (errno == EINTR) {
            printf("interrupted\n");
            return;
        }

        if (ret < 0) continue; // try again

        cmsg = CMSG_FIRSTHDR(msg);
        if (!cmsg) {
            return;
        }
        if (cmsg->cmsg_level == SOL_IP
            && cmsg->cmsg_type == IP_RECVERR) {
            sock_err = (struct sock_extended_err *)CMSG_DATA(cmsg);

            if (!sock_err) return;

            switch (sock_err->ee_type)
            {
            case ICMP_NET_UNREACH:
                fprintf(stderr, "Network Unreachable Error\n");
                break;
            case ICMP_HOST_UNREACH:
                fprintf(stderr, "Host Unreachable Error\n");
                break;
                /* Handle all other cases. Find more errors :
                 * http://lxr.linux.no/linux+v3.5/include/linux/icmp.h#L39
                 */
            case ICMP_NET_UNR_TOS:
                g_tcrt_ctx.rc = TTLEXCEEDED;
                break;
            case ICMP_UNREACH_PORT:
                g_tcrt_ctx.rc = HAVEANSWER;
                break;
            }

            g_tcrt_ctx.answer_ip = ((struct sockaddr_in*)SO_EE_OFFENDER(sock_err))->sin_addr.s_addr;
//            printf("%u\n", sock_err->offender.sin_addr.s_addr);

//            printf("rettype: %d\n", sock_err->ee_type);
        }
        else {
            printf("unknown\n");
        }
        break;
    }
}

static void trcrt_print_result() {
    char hostname[NI_MAXHOST] = { 0 };
    bool in_cache;

    get_name_by_ipaddr(g_tcrt_ctx.answer_ip, hostname, sizeof hostname, &in_cache);

    if (!in_cache) {
        printf("%s ", hostname);
        printf("(%s)  ", inet_ntoa((struct in_addr){g_tcrt_ctx.answer_ip}));
    }

    switch (g_tcrt_ctx.rc)
    {
    case HAVEANSWER:
        printf("_time_  ");
        break;
    case TTLEXCEEDED:
        printf("_ttl_  ");
        break;
    default:
        printf("*  ");
        break;
    }
}