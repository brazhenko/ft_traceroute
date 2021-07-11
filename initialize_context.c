#include "traceroute.h"
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>

traceroute_context_t g_tcrt_ctx;

/*
 * Static prototypes
 *
 */

static void dump_usage(const char *bin_name);
static void dump_version();
static void set_default_args();
static void initialize_signals();

/*
 * Dns resolvers
 *
 */

int get_ipaddr_by_name(const char *name, in_addr_t *out,
        char *canon_name, size_t canon_name_size);
int get_name_by_ipaddr(in_addr_t ip, char *host,
        size_t host_len, bool *in_cache);

/*
 * Function: initialize_context()
 * ----------------------------
 *  Handles programm's arguments and fills global
 *  structure g_tcrt_ctx;
 *
 *  argc - argument count
 *
 *  argv - argument vector
 *
 *  returns: no return
 *
 */

void initialize_context(int argc, char **argv) {
    set_default_args();

    /* options descriptor */
    static struct option long_opts[] = {
        { "help",       no_argument,        NULL,   'h' },
        { "first",      required_argument,  NULL,   'f' },
        { "icmp",       no_argument,        NULL,   'I' },
        { "port",       required_argument,  NULL,   'p' },
        { "sendwait",   required_argument,  NULL,   'z' },
        { "max-hops",   required_argument,  NULL,   'm' },
        { "tos",        required_argument,  NULL,   't' },
        { "version",    required_argument,  NULL,   'V' },
        { "queries",    required_argument,  NULL,   'q' },
        { "wait",       required_argument,  NULL,   'w' },
        { NULL,         0,                  NULL,    0  }
    };

    int ch, ret;
    while ((ch = getopt_long(argc, argv,
            "hITp:z:m:t:w:", long_opts, NULL)) != -1) {
        switch (ch) {
        case 'f':
            g_tcrt_ctx.current_ttl = atoi(optarg);
            break;
        case 'I':
            g_tcrt_ctx.flags |= TRCRT_ICMP;
            break;
        case 'p':
            g_tcrt_ctx.dest_port = atoi(optarg);
            break;
        case 'z':
            g_tcrt_ctx.send_wait = atoi(optarg);
            break;
        case 'm':
            g_tcrt_ctx.max_ttl = atoi(optarg);
            break;
        case 't':
            g_tcrt_ctx.tos = atoi(optarg);
            break;
        case 'q':
            g_tcrt_ctx.query_count = atoi(optarg);
            if (g_tcrt_ctx.query_count == 0
            || g_tcrt_ctx.query_count > MAX_PROBS_PER_HOP) {
                fprintf(stderr, "no more than %d probes per hop\n", MAX_PROBS_PER_HOP);
                exit(EXIT_FAILURE);
            }
        case 'w':
            g_tcrt_ctx.wait_max = atoi(optarg);
            break;
        case 'v':
            dump_version();
            exit(EXIT_SUCCESS);
        case 'h':
            dump_usage(argv[0]);
            exit(EXIT_SUCCESS);
        default:
            fprintf(stderr, "Unsupported option\n");
            exit(EXIT_FAILURE);
        }
    }

    if (optind == argc) {
        fprintf(stderr, "Specify \"host\" missing argument.\n");
        exit(EXIT_FAILURE);
    }

    if (optind + 2 < argc) {
        fprintf(stderr, "Extra arg `%s' (position %d, argc %d)\n",
                argv[optind + 2], optind + 2, argc - 1);
        exit(EXIT_FAILURE);
    }

    // Prepare dest IP address
    ret = get_ipaddr_by_name(argv[optind], &g_tcrt_ctx.dest_ip, NULL, 0);
    if (ret) {
        fprintf(stderr, "%s: %s\n", argv[0], gai_strerror(ret));
        exit(EXIT_FAILURE);
    }

    // Provide dest name
    g_tcrt_ctx.dest_name = argv[optind];

    // Get packet len if exists
    if (optind + 1 < argc) {
        g_tcrt_ctx.pack_len = atoi(argv[optind + 1]);
        if (!(0 < g_tcrt_ctx.pack_len && g_tcrt_ctx.pack_len < MAX_PACK_LEN_INPUT)) {
            fprintf(stderr, "no more than %d packet len\n", MAX_PACK_LEN_INPUT);
            exit(EXIT_FAILURE);
        }
    }

    initialize_signals();
}

static void alarm_handler(int unused) {
    (void)unused;
    g_tcrt_ctx.try_read = 0;
}

static void initialize_signals() {
    struct sigaction sa;
    memset(&sa, 0x0, sizeof sa);

    sa.sa_handler = alarm_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGALRM, &sa, NULL);
}

static void dump_usage(const char *bin_name) {
    fprintf(stderr,
    "Usage:\n"
    "%s "
    "[ -hIT ] "
    "[ -i device ] "
    "[ -p port ] "
    "[ -z sendwait ] "
    "[ -m max_ttl ] "
    "[ -t tos ] "
    "host "
    "[ packetlen ]\n"
    "Options:\n"
    "  -f first_ttl  --first=first_ttl\n"
    "                              Start from the first_ttl hop (instead from 1)\n"
    "  -I  --icmp                  Use ICMP ECHO for tracerouting\n"
    "  -m max_ttl  --max-hops=max_ttl\n"
    "                              Set the max number of hops (max TTL to be\n"
    "                              reached). Default is 30\n"
    "  -p port  --port=port        Set the destination port to use. It is either\n"
    "                              initial udp port value for \"default\" method\n"
    "                              (incremented by each probe, default is 33434), or\n"
    "                              initial seq for \"icmp\" (incremented as well,\n"
    "                              default from 1), or some constant destination\n"
    "                              port for other methods (with default of 80 for\n"
    "                              \"tcp\", 53 for \"udp\", etc.)\n"
    "  -q nqueries  --queries=nqueries\n"
    "                              Set the number of probes per each hop. Default is\n"
    "                              3\n"
    "  -t tos  --tos=tos           Set the TOS (IPv4 type of service) value for outgoing packets\n"
    "  -w MAX  --wait=MAX          Wait for a probe no more than MAX (default 5.0)\n"
    "                              seconds\n"
    "  -z sendwait  --sendwait=sendwait\n"
    "                              Minimal time interval between probes (default 0).\n"
    "                              If the value is more than 10, then it specifies a\n"
    "                              number in milliseconds, else it is a number of\n"
    "                              seconds (float point values allowed too)\n"
    "  -V  --version               Print version info and exit\n"
    "  -h  --help                  Read this help and exit\n"
    "\n"
    "Arguments:\n"
    "+     host          The host to traceroute to\n"
    "      packetlen     The full packet length (default is the length of an IP\n"
    "                    header plus 40). Can be ignored or increased to a minimal\n"
    "                    allowed value\n", bin_name);
}

static void set_default_args() {
    memset(&g_tcrt_ctx, 0x0, sizeof g_tcrt_ctx);

    if (g_tcrt_ctx.flags & TRCRT_ICMP) {
        g_tcrt_ctx.dest_port = 1; /* sequence id */
    }
    else {
        g_tcrt_ctx.dest_port = DEFAULT_START_PORT;
    }

    g_tcrt_ctx.send_wait = 0;
    g_tcrt_ctx.max_ttl = DEFAULT_MAX_TTL;
    g_tcrt_ctx.current_ttl = 1;
    g_tcrt_ctx.query_count = DEFAULT_PROBES_ONE_TTL;
    g_tcrt_ctx.wait_max = DEFAULT_RESP_TIME_WAIT;
    g_tcrt_ctx.pack_len = DEFAULT_PACK_LEN;
}

static void dump_version() {
    printf("%s built on %s at %s\n", "ft_tracroute v0.0.1", __DATE__, __TIME__);
}
