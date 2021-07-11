#ifndef FT_TRACEROUTE_TRACEROUTE_H
# define FT_TRACEROUTE_TRACEROUTE_H

# include <stddef.h>
# include <stdint.h>
# include <netinet/in.h>
#include <netdb.h>

typedef struct {
    int sock;
    uint64_t flags;
    const char *device;
    char* dest_name;
    in_addr_t dest_ip;
    in_addr_t source_ip;
    uint16_t source_port;
    unsigned send_wait;
    uint8_t max_ttl;
    uint8_t tos;
    int pack_len;
    uint8_t query_count;
    uint8_t wait_max;
    // Dynamic data
    uint8_t current_ttl;
    uint16_t dest_port;
    int icmp_rpl_type;
    int icmp_rpl_code;
    int end_tracing;
    in_addr_t answer_ip;
    struct timeval time_sent;
    int try_read;
    in_addr_t last_printed_ip;
} traceroute_context_t;

extern traceroute_context_t g_tcrt_ctx;

/* Flags */
# define TRCRT_ICMP 0x1         /* Use ICMP instead default UDP */

/* Value constants */
# define DEFAULT_START_PORT 33434
# define DEFAULT_PROBES_ONE_TTL 3
# define DEFAULT_RESP_TIME_WAIT 5
# define DEFAULT_MAX_TTL 30
# define MICROSECONDS_IN_SECOND 1000000
# define DEFAULT_PACK_LEN 60
# define MAX_PROBS_PER_HOP 10
# define MAX_PACK_LEN_INPUT 1000

#define max(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

void initialize_context(int argc, char **argv);

#endif
