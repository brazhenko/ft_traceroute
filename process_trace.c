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
#include <linux/udp.h>

/*
 * Prototypes
 *
 */

static void trcrt_send();
static void initialize_socket();
static void destroy_socket();

static void trcrt_receive();

static void trcrt_handle_icmp();
static void trcrt_handle_udp();
static void trcrt_print_result();

static __suseconds_t time_diff(struct timeval* begin, struct timeval *end);

int send_icmp_msg_v4(int sock, uint8_t tos, uint16_t id, uint8_t ttl, uint8_t icmp_type,
        uint16_t icmp_seq_num, size_t payload_size, in_addr_t source_ip,
        in_addr_t dest_ip);
int send_udp_trcrt_msg(int udp_sock, uint8_t tos, uint8_t ttl, size_t payload_size,
        in_addr_t dest_ip, in_port_t dest_port);
int get_name_by_ipaddr(in_addr_t ip, char *host,
        size_t host_len, bool *in_cache);

/*
 * Main logic
 *
 */

void process_trace() {
    // Go, go, go !!!
    printf("traceroute to %s (%s), %d hops max, %d byte packets\n",
            g_tcrt_ctx.dest_name,
            inet_ntoa((struct in_addr){ g_tcrt_ctx.dest_ip }),
            g_tcrt_ctx.max_ttl, g_tcrt_ctx.pack_len);

    for (
            int i = 1;
            g_tcrt_ctx.current_ttl < g_tcrt_ctx.max_ttl;
            g_tcrt_ctx.current_ttl++, i++) {

        printf("%2d  ", i);
        for (uint8_t j = 0; j < g_tcrt_ctx.query_count; j++, g_tcrt_ctx.dest_port++) {
            // Pipeline
            initialize_socket();
            trcrt_send();
            trcrt_receive();
            trcrt_print_result();
            destroy_socket();

            sleep(g_tcrt_ctx.send_wait);
        }
        printf("\n");

        if (g_tcrt_ctx.end_tracing) break;
    }
}

static void trcrt_send() {
    int ret;

    if (g_tcrt_ctx.flags & TRCRT_ICMP) {
        ret = send_icmp_msg_v4(
                g_tcrt_ctx.sock,
                g_tcrt_ctx.tos,
                getpid(),
                g_tcrt_ctx.current_ttl,
                ICMP_ECHO,
                g_tcrt_ctx.dest_port /* according to man */,
                max(g_tcrt_ctx.pack_len - (int)sizeof (struct iphdr) - (int)sizeof (struct icmphdr), 0),
                g_tcrt_ctx.source_ip,
                g_tcrt_ctx.dest_ip);
    }
    else {
        ret = send_udp_trcrt_msg(
                g_tcrt_ctx.sock,
                g_tcrt_ctx.tos,
                g_tcrt_ctx.current_ttl,
                max(g_tcrt_ctx.pack_len - (int)sizeof (struct iphdr) - (int)sizeof (struct udphdr), 0),
                g_tcrt_ctx.dest_ip,
                g_tcrt_ctx.dest_port);
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

static void initialize_socket() {
    int sock, setsock;

    if (g_tcrt_ctx.flags & TRCRT_ICMP) {
        sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    }
    else {
        sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    }

    if (sock < 0) {
        perror("cannot create socket");
        exit(EXIT_FAILURE);
    }

    if (g_tcrt_ctx.flags & TRCRT_ICMP)
        setsock = setsockopt(sock, IPPROTO_IP, IP_HDRINCL, (int[1]){ 1 }, sizeof(int));
    else {
        setsock = setsockopt(sock, SOL_IP, IP_RECVERR, (int[1]){ 1 }, sizeof(int));
    }

    if (setsock < 0) {
        perror("cannot set sock option");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Set socket to context
    g_tcrt_ctx.sock = sock;
}

static void destroy_socket() {
    if (close(g_tcrt_ctx.sock) != 0) {
        perror("cannot close socket");
        exit(EXIT_FAILURE);
    }
}

static void trcrt_receive() {
    alarm(g_tcrt_ctx.wait_max);

    if (g_tcrt_ctx.flags & TRCRT_ICMP) {
        trcrt_handle_icmp();
    }
    else {
        trcrt_handle_udp();
    }
}

static void trcrt_handle_icmp() {
    struct icmphdr* icmp;
    struct iphdr* ip;
    char   buffer[2048];
    ssize_t ret;

    ret = recvfrom(g_tcrt_ctx.sock, buffer, sizeof buffer, 0, NULL, 0);

    // Answer came
    if (ret > 0) {
        ip = (struct iphdr *)buffer;
        icmp = (struct icmphdr*)(buffer + sizeof (struct iphdr));

        g_tcrt_ctx.answer_ip = ip->saddr;
        g_tcrt_ctx.icmp_rpl_type = icmp->type;
        g_tcrt_ctx.icmp_rpl_code = icmp->code;
        if (icmp->type == ICMP_ECHOREPLY || icmp->type == ICMP_DEST_UNREACH) {
            g_tcrt_ctx.end_tracing = 1;
        }
        return;
    }

    // Some unexpected error
    if (ret == 0) {
        fprintf(stderr, "Connection closed\n");
        exit(EXIT_FAILURE);
    }

    // Reading interrupted by signal, equivalent to (errno==EINTR)
    g_tcrt_ctx.icmp_rpl_type = -1;
}

static void trcrt_handle_udp() {

    char   buffer[2048];
    struct iovec   iov[1];
    struct msghdr  msg;
    ssize_t ret;

    // Init message struct
    memset(&msg, 0x0, sizeof(msg));
    memset(iov, 0x0, sizeof(iov));
    iov[0].iov_base = buffer;
    iov[0].iov_len = sizeof buffer;
    msg.msg_iov     = iov;
    msg.msg_iovlen  = 1;

    char    buffer2[1024];
    struct cmsghdr *cmsg;
    struct sock_extended_err *sock_err;

    msg.msg_flags = 0;

    msg.msg_control = buffer2;
    msg.msg_controllen = sizeof(buffer2);

    g_tcrt_ctx.try_read = 1;

    while (g_tcrt_ctx.try_read) {
        ret = recvmsg(g_tcrt_ctx.sock, &msg, MSG_ERRQUEUE);
        if (ret < 0) continue; // Resp have not come yet, try again...

        cmsg = CMSG_FIRSTHDR(&msg);

        if (cmsg->cmsg_level == SOL_IP && cmsg->cmsg_type == IP_RECVERR) {
            sock_err = (struct sock_extended_err *)CMSG_DATA(cmsg);

            g_tcrt_ctx.icmp_rpl_type = sock_err->ee_type;
            g_tcrt_ctx.icmp_rpl_code = sock_err->ee_code;
            if (sock_err->ee_type == ICMP_ECHOREPLY || sock_err->ee_code == ICMP_DEST_UNREACH) {
                g_tcrt_ctx.end_tracing = 1;
            }

            // Get sender's IP
            g_tcrt_ctx.answer_ip = ((struct sockaddr_in*)SO_EE_OFFENDER(sock_err))->sin_addr.s_addr;
        }
        else {
            printf("unknown\n");
        }

        return;
    }

    g_tcrt_ctx.icmp_rpl_type = -1;
}

static void trcrt_print_result() {
    char hostname[NI_MAXHOST] = { 0 };
    struct timeval msg_rcv_time;
    __suseconds_t rtt;
    int ret;

    if (gettimeofday(&msg_rcv_time, NULL) != 0) {
        perror("cannot set time of receiving");
        exit(EXIT_FAILURE);
    }
    ret = get_name_by_ipaddr(g_tcrt_ctx.answer_ip, hostname, sizeof hostname, NULL);
    rtt = time_diff(&g_tcrt_ctx.time_sent, &msg_rcv_time);

    if (g_tcrt_ctx.answer_ip != g_tcrt_ctx.last_printed_ip) {
        if (ret == 0) {
            // managed to resolve hostname
            printf("%s ", hostname);
        }
        else {
            // Did not manage to resolve hostname
            printf("%s ", inet_ntoa((struct in_addr){ g_tcrt_ctx.answer_ip }));
        }

        printf("(%s)  ", inet_ntoa((struct in_addr){ g_tcrt_ctx.answer_ip }));
        g_tcrt_ctx.last_printed_ip = g_tcrt_ctx.answer_ip;
    }

    switch (g_tcrt_ctx.icmp_rpl_type)
    {
    case ICMP_ECHOREPLY:
    case ICMP_DEST_UNREACH:
    case ICMP_TIME_EXCEEDED:
        printf("%ld.%03ld ms  ", rtt / 1000, rtt % 1000);
        break;
    case -1:
        printf("*  ");
        break;
    default:
        printf("#  ");
    }

    if (g_tcrt_ctx.icmp_rpl_type ==  ICMP_DEST_UNREACH)
        switch (g_tcrt_ctx.icmp_rpl_code)
        {
        case ICMP_NET_UNREACH:
            printf("!N  ");
            break;
        case ICMP_HOST_UNREACH:
            printf("!H  ");
            break;
        case ICMP_PROT_UNREACH:
            printf("!P  ");
            break;
        case ICMP_PORT_UNREACH:
            break; // Default case, no reaction
        case ICMP_NET_ANO:
        default:
            printf("!X  ");
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