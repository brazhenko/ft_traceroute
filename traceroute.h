#ifndef FT_TRACEROUTE_TRACEROUTE_H
# define FT_TRACEROUTE_TRACEROUTE_H

# include <stddef.h>
# include <stdint.h>

typedef struct {
    int sock;
    uint64_t flags;
    const char *device;
    int dest_port;
    int source_port;
    unsigned send_wait;
    uint8_t max_ttl;
    uint8_t tos;
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




void initialize_context(int argc, char **argv);

#endif
