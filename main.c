#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>
#include <string.h>
#include <netinet/in.h>
#include <string.h>
#include <netdb.h>
#include "traceroute.h"
#include <signal.h>
#include <asm-generic/errno.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#define SO_EE_ORIGIN_NONE    0
#define SO_EE_ORIGIN_LOCAL   1
#define SO_EE_ORIGIN_ICMP    2
#define SO_EE_ORIGIN_ICMP6   3

struct sock_extended_err {
    uint32_t ee_errno;   /* error number */
    uint8_t  ee_origin;  /* where the error originated */
    uint8_t  ee_type;    /* type */
    uint8_t  ee_code;    /* code */
    uint8_t  ee_pad;
    uint32_t ee_info;    /* additional information */
    uint32_t ee_data;    /* other data */
    /* More data may follow */
};

#define BUFFER_MAX_SIZE 1024



void f(int a){
    printf("alarm\n");
}

int send_udp_trcrt_msg(
        int udp_sock,
        uint8_t ttl,
        size_t payload_size,
        in_addr_t dest_ip,
        in_port_t dest_port
);


int main(int ac, char **av)
{
    initialize_context(ac, av);


    in_addr_t addr;
//    int ret = get_ipaddr_by_name("ya.ru", &addr, NULL, 0);
    addr = 4076534359; // ya.ru

//    if (ret) {
//        fprintf(stderr, "%s\n", gai_strerror(ret));
//        return 1;
//    }

    struct sigaction sa;

    sa.sa_handler = f;
    sigemptyset(&sa.sa_mask);
//    sa.sa_flags |= SA_RESTART;
    sigaction(SIGALRM, &sa, NULL);

    send_udp_trcrt_msg(g_tcrt_ctx.sock, 5, 32, addr, 33434);

    int on = 1;
/* Set the option, so we can receive errors */
    setsockopt(g_tcrt_ctx.sock, SOL_IP, IP_RECVERR, (char *)&on, sizeof(on));
    int return_status;
    char buffer[BUFFER_MAX_SIZE];
    struct iovec iov;                       /* Data array */
    struct msghdr msg;                      /* Message header */
    struct cmsghdr *cmsg;                   /* Control related data */
    struct sock_extended_err *sock_err;     /* Struct describing the error */
    struct icmphdr icmph;                   /* ICMP header */
    struct sockaddr_in remote;              /* Our socket */

    for (;;)
    {
        iov.iov_base = &icmph;
        iov.iov_len = sizeof(icmph);
        msg.msg_name = (void *)&remote;
        msg.msg_namelen = sizeof(remote);
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        msg.msg_flags = 0;
        msg.msg_control = buffer;
        msg.msg_controllen = sizeof(buffer);
        /* Receiving errors flog is set */
        return_status = recvmsg(g_tcrt_ctx.sock, &msg, MSG_ERRQUEUE);
        if (return_status < 0)
            continue;
        perror("Govno");

        for (cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg))
        {
            /* Ip level */
            if (cmsg->cmsg_level == SOL_IP)
            {
                /* We received an error */
                if (cmsg->cmsg_type == IP_RECVERR)
                {
                    fprintf(stderr, "We got IP_RECVERR message\n");
                    sock_err = (struct sock_extended_err *)CMSG_DATA(cmsg);
                    if (sock_err)
                    {
                        /* We are intrested in ICMP errors */
                        if (sock_err->ee_origin == SO_EE_ORIGIN_ICMP)
                        {
                            /* Handle ICMP errors types */
                            switch (sock_err->ee_type)
                            {
                            case ICMP_NET_UNREACH:
                                /* Hendle this error */
                                fprintf(stderr, "Network Unreachable Error\n");
                                break;
                            case ICMP_HOST_UNREACH:
                                /* Hendle this error */
                                fprintf(stderr, "Host Unreachable Error\n");
                                break;
                                /* Handle all other cases. Find more errors :
                                 * http://lxr.linux.no/linux+v3.5/include/linux/icmp.h#L39
                                 */

                            }

                            printf("errtype: %d\n", sock_err->ee_type);
                        }
                    }
                }
            }
        }

        exit(1);
    }


}