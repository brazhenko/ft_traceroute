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
void process_trace();


int main(int ac, char **av)
{
    struct sigaction sa;
    sa.sa_handler = f;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGALRM, &sa, NULL);


    initialize_context(ac, av);
    process_trace();

    return 0;

    in_addr_t addr;
//    int ret = get_ipaddr_by_name("ya.ru", &addr, NULL, 0);
    addr = 4076534359; // ya.ru



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

        printf("%d\n", return_status);
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
                        if (sock_err->ee_origin == SO_EE_ORIGIN_ICMP)
                        {
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
                            printf("errtype: %d\n", sock_err->ee_type);

                        }
                    }
                }
            }
        }

        exit(1);
    }


}


//char arr[] = "\x45\x00\x00\x3c\x89\x74\x00\x00\x01\x01\x31\x4d\xac\x11\x00\x02\n"
//             "\x57\xfa\xfa\xf2\x08\x00\x82\x30\x00\x49\x00\x01\x48\x49\x4a\x4b\n"
//             "\x4c\x4d\x4e\x4f\x50\x51\x52\x53\x54\x55\x56\x57\x58\x59\x5a\x5b\n"
//             "\x5c\x5d\x5e\x5f\x60\x61\x62\x63\x64\x65\x66\x67";
//struct iphdr* ip = arr;
//struct icmphdr *icmp = arr + sizeof (struct iphdr);
//
//char arr2[] = "\x45\x00\x00\x4e\x97\x2c\x40\x00\x40\x01\xa4\x82\xac\x11\x00\x02"
//              "\x57\xfa\xfa\xf2\x45\x00\x00\x3a\x12\x01\x00\x00\x01\x01\xa8\xc4"
//              "\x57\xfa\xfa\xf2\x57\xfa\xfa\xf2\x08\x00\x2b\xdf\x01\x12\x00\x42"
//              "\xcf\x06\xe8\x60\x00\x00\x00\x00\x3b\x95\x08\x00\x00\x00\x00\x00"
//              "\x42\x42\x42\x42\x42\x42\x42\x42\x42\x42\x42\x42\x42\x42";
//
//struct iphdr* ip2 = arr2;
//struct icmphdr *icmp2 = arr2 + sizeof (struct iphdr);
