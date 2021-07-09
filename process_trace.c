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
#include "traceroute.h"


/*
 * Prototypes
 *
 */
static void trcrt_send();
static void trcrt_receive();
static void trcrt_print_result();

int send_icmp_msg_v4(int sock, uint16_t id, uint8_t ttl, uint8_t icmp_type,
        uint16_t icmp_seq_num, size_t payload_size, in_addr_t source_ip,
        in_addr_t dest_ip);
int send_udp_trcrt_msg(int udp_sock, uint8_t ttl, size_t payload_size,
        in_addr_t dest_ip, in_port_t dest_port);


void process_trace() {

    for (; g_tcrt_ctx.current_ttl < g_tcrt_ctx.max_ttl; g_tcrt_ctx.current_ttl++) {
        printf("------- %d ------\n", g_tcrt_ctx.current_ttl);
        for (int i = 0; i < DEFAULT_PROBES_ONE_TTL; i++) {
            trcrt_send();
            trcrt_receive();
            trcrt_print_result();
            sleep(g_tcrt_ctx.send_wait);
        }

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
                ICMP_ECHO, // TODO lookup code
                0x42,
                30,
                g_tcrt_ctx.source_ip, // TODO fix for source
                g_tcrt_ctx.dest_ip);
    }
    else if (g_tcrt_ctx.flags & TRCRT_TCP)
        ;
    else {
        ret = send_udp_trcrt_msg(
                g_tcrt_ctx.sock,
                g_tcrt_ctx.current_ttl,
                30, // TODO FIX
                g_tcrt_ctx.dest_ip,
                g_tcrt_ctx.dest_port);
        printf("kek\n");
    }

    if (ret != 0) {
        perror("cannot send msg");
        exit(EXIT_FAILURE);
    }

}

static void trcrt_receive() {
    char    output[1024], ip_buffer[64], host_name[NI_MAXHOST], buffer[2048], buffer2[1024];
    struct cmsghdr *cmsg;
    struct sock_extended_err *sock_err;
    struct timeval  current_time, send_time;
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
        ret = recvmsg(g_tcrt_ctx.sock, &msg, 0);
        if (ret > 0) {
            struct iphdr* ip = (struct iphdr *)buffer;
            struct icmphdr* icmp = (struct icmphdr*)(buffer + sizeof (struct iphdr));
            g_tcrt_ctx.answer_ip = ip->saddr;
            if (icmp->type == ICMP_ECHOREPLY) {
                g_tcrt_ctx.rc = HAVEANSWER;
            }
            else if (icmp->type == ICMP_TIME_EXCEEDED) {
                g_tcrt_ctx.rc = TTLEXCEEDED;
            }
            else {
                g_tcrt_ctx.rc = UNKNOWN;
            }
            printf("icmp: %d\n", icmp->type);
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
    else if (g_tcrt_ctx.flags & TRCRT_TCP)
        ;
    else {
        while (true) {
            int on = 1;
            ret = setsockopt(g_tcrt_ctx.sock, SOL_IP, IP_RECVERR, (char *)&on, sizeof(on));
            if (ret != 0) {
                perror("shit");
                exit(EXIT_FAILURE);
            }
            struct sockaddr_in remote;              /* Our socket */
            msg.msg_name = (void *)&remote;
            msg.msg_namelen = sizeof(remote);
            msg.msg_flags = 0;
            msg.msg_control = buffer2;
            msg.msg_controllen = sizeof(buffer2);

            ret = recvmsg(g_tcrt_ctx.sock, &msg, MSG_ERRQUEUE);
//            printf("%d %d %s\n", ret, errno, strerror(errno));

            if (errno == EINTR) {
                printf("interrupted\n");
                return;
            }

            if (ret < 0) continue; // try again

            printf("Hello\n");

            cmsg = CMSG_FIRSTHDR(&msg);
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
                }

                printf("rettype: %d\n", sock_err->ee_type);
            }

            break;
        }
    }

    if (ret < 0) {
        perror("cannot create socket");
        exit(EXIT_FAILURE);
    }
}

static void trcrt_print_result() {

}