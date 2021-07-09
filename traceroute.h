#ifndef FT_TRACEROUTE_TRACEROUTE_H
# define FT_TRACEROUTE_TRACEROUTE_H

# include <stddef.h>
# include <stdint.h>
# include <netinet/in.h>

enum ret_code {
    NOANSWER=1,
    HAVEANSWER,
    TTLEXCEEDED,
    LOST,
    UNKNOWN
};

typedef struct {
    int sock;
    uint64_t flags;
    const char *device;
    in_addr_t dest_ip;
    int dest_port;
    in_addr_t source_ip;
    int source_port;
    unsigned send_wait;
    uint8_t max_ttl;
    uint8_t tos;
    int pack_len;
    // Dynamic data
    uint8_t current_ttl;
    enum ret_code rc;
    in_addr_t answer_ip;
} traceroute_context_t;

extern traceroute_context_t g_tcrt_ctx;

/* Flags */
# define TRCRT_ICMP 0x1         /* Use ICMP instead default UDP */
# define TRCRT_TCP 0x2          /* Use TCP instead od default UDP or ICMP */
# define TRCRT_INTERFACE 0x4
# define TRCRT_PORT 0x8         /* Set start port instead of default 33434 */
# define TRCRT_SENDWAIT 0x10    /* Set interval between probes (default 0) */
# define TRCRT_MAXHOPS 0x20     /* Max ttl option */
# define TRCRT_SOURCE_IP 0x40
# define TRCRT_TOS 0x80         /* Set type of service */
# define TRCRT_SOURCEPORT 0x100 /* Source port enabled */

/* Value constants */
# define DEFAULT_START_PORT 33434   /* Default start port */
# define DEFAULT_PROBES_ONE_TTL 3


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

void initialize_context(int argc, char **argv);

#endif
