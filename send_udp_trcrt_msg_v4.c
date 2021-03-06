#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/ip_icmp.h>
#include <netdb.h>

/*
 * Function: send_udp_trcrt_msg()
 * ------------------------------
 *  Makes up a UDPv4 message
 *  and sends to a particular IP-address and port
 *
 *  udp_sock - socket(AF_INET, SOCK_DGRAM, IPPROTO_IP)
 *
 *  tos - type of service
 *
 *  ttl - tll for packet
 *
 *  payload_size - size of udp buffer sent (random data)
 *
 *  dest_ip - destination IPv4 address
 *
 *  dest_port - destination port (for traceroute msgs
 *  supposed to be unused, e.g. 33434 and greater)
 *
 *  returns:    0 - success
 *              1 - error, errno will be set in a particular errcode
 *
 */

int send_udp_trcrt_msg(
        int udp_sock,
        uint8_t tos,
        uint8_t ttl,
        size_t payload_size,
        in_addr_t dest_ip,
        in_port_t dest_port
) {
    ssize_t ret;
    // Prepare destination
    struct sockaddr_in dest;
    memset(&dest, 0x0, sizeof dest);
    dest.sin_family = AF_INET;
    dest.sin_addr.s_addr = dest_ip;
    dest.sin_port = htons(dest_port);

    struct sockaddr_in source;
    memset(&source, 0x0, sizeof source);

    // Set TTL
    if (setsockopt(udp_sock, SOL_IP, IP_TTL, &ttl, sizeof ttl) != 0) {
        return 1;
    }
    // Set TOS
    if (setsockopt(udp_sock, SOL_IP, IP_TOS, &tos, sizeof tos) != 0) {
        return 1;
    }

    // Prepare send buffer
    char    buffer[payload_size];
    memset(buffer, 0x42, payload_size);
    ret = sendto(udp_sock, buffer, payload_size, 0,
            (struct sockaddr *)&dest, sizeof dest);
    if (ret == -1) {
        return 1;
    }

    return 0;
}
