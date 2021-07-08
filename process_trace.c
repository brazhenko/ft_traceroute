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

static void trcrt_send();
static void trcrt_receive();

void process_trace() {
    for (uint8_t ttl = 1; ttl < g_tcrt_ctx.max_ttl; ttl++) {
        for (int i = 0; i < 3 /*hardcode*/; i++) {
            trcrt_send();
            trcrt_receive();
            // print result
            sleep(g_tcrt_ctx.send_wait);
        }
    }
}

int send_icmp_msg_v4(
        int sock,
        uint16_t id,
        uint8_t ttl,
        uint8_t icmp_type,
        uint16_t icmp_seq_num,
        size_t payload_size,
        in_addr_t source_ip,
        in_addr_t dest_ip);

int send_udp_trcrt_msg(
        int udp_sock,
        uint8_t ttl,
        size_t payload_size,
        in_addr_t dest_ip,
        in_port_t dest_port);

static void trcrt_send() {
    int ret = 0;

    if (g_tcrt_ctx.flags & TRCRT_ICMP) {
        ret = send_icmp_msg_v4(
                g_tcrt_ctx.sock,
                getpid(),
                g_tcrt_ctx.current_ttl,
                30, // TODO lookup code
                0x42,
                30,
                g_tcrt_ctx.dest_ip, // TODO fix for source
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
    }

    if (ret < 0) {
        perror("cannot create socket");
        exit(EXIT_FAILURE);
    }

}

static void trcrt_receive() {

    char    output[1024], ip_buffer[64], host_name[NI_MAXHOST], buffer[2048];
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
        if (errno == EINTR) {
            // interrupted by timer
        }

    }
    else if (g_tcrt_ctx.flags & TRCRT_TCP)
        ;
    else {
        while (true) {
            ret = recvmsg(g_tcrt_ctx.sock, &msg, MSG_ERRQUEUE);
            if (errno == EINTR) {
                // interrupted by timer
                return;
            }

            if (ret < 0) continue; // try again

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
            }
        }
    }

    if (ret < 0) {
        perror("cannot create socket");
        exit(EXIT_FAILURE);
    }
}