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
    in_addr_t source_ip;
    uint16_t source_port;
    unsigned send_wait;
    uint8_t max_ttl;
    uint8_t tos;
    int pack_len;
    uint8_t query_count;
    // Dynamic data
    uint8_t current_ttl;
    uint16_t dest_port;
    enum ret_code rc;
    int end_tracing;
    in_addr_t answer_ip;
    struct timeval time_sent;
    int try_read;
    in_addr_t last_printed_ip;
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
# define TRCRT_FIRST_TTL 0x200 /* First ttl enabled (default 1) */
# define TRCRT_QUERY_COUNT 0x400 /* Nqueries is set */

/* Value constants */
# define DEFAULT_START_PORT 33434
# define DEFAULT_PROBES_ONE_TTL 3
# define DEFAULT_RESP_TIME_WAIT 5
# define MICROSECONDS_IN_SECOND 1000000

void initialize_context(int argc, char **argv);

#endif
