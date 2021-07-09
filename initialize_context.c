#include "traceroute.h"
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>

traceroute_context_t g_tcrt_ctx;

static void dump_usage(const char *bin_name);
static void dump_version();
static void set_default_args();
static void initialize_sockets();
static void initialize_signals();

int get_ipaddr_by_name(const char *name, in_addr_t *out,
        char *canon_name, size_t canon_name_size);

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
        { "icmp",       no_argument,        NULL,   'I' },
        { "tcp",        no_argument,        NULL,   'T' },
        { "interface",  required_argument,  NULL,   'i' },
        { "port",       required_argument,  NULL,   'p' },
        { "sendwait",   required_argument,  NULL,   'z' },
        { "max-hops",   required_argument,  NULL,   'm' },
        { "source",     required_argument,  NULL,   's' },
        { "tos",        required_argument,  NULL,   't' },
        { "sport",      required_argument,  NULL,    1  },
        { "version",    required_argument,  NULL,   'V' },
        { NULL,         0,                  NULL,    0  }
    };

    int ch;
    while ((ch = getopt_long(argc, argv,
            "hITi:p:z:m:s:t:", long_opts, NULL)) != -1) {
        switch (ch) {
        case 'I':
            g_tcrt_ctx.flags |= TRCRT_ICMP;
            break;
        case 'T':
            g_tcrt_ctx.flags |= TRCRT_TCP;
            break;
        case 'i':
            g_tcrt_ctx.flags |= TRCRT_INTERFACE;
            g_tcrt_ctx.device = optarg;
            break;
        case 'p':
            g_tcrt_ctx.flags |= TRCRT_PORT;
            g_tcrt_ctx.dest_port = atoi(optarg);
            break;
        case 'z':
            g_tcrt_ctx.flags |= TRCRT_SENDWAIT;
            g_tcrt_ctx.send_wait = atoi(optarg);
            break;
        case 'm':
            g_tcrt_ctx.flags |= TRCRT_MAXHOPS;
            g_tcrt_ctx.max_ttl = atoi(optarg);
            break;
        case 's':
            g_tcrt_ctx.flags |= TRCRT_SOURCE_IP; // TODO
            break;
        case 't':
            g_tcrt_ctx.flags |= TRCRT_TOS;
            g_tcrt_ctx.tos = atoi(optarg);
            break;
        case 1:
            g_tcrt_ctx.flags |= TRCRT_SOURCEPORT;
            break;
        case 'v':
            dump_version();
            exit(EXIT_SUCCESS);
        case 'h':
            dump_usage(argv[0]);
            exit(EXIT_SUCCESS);
        default:
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
    int ret = get_ipaddr_by_name(argv[optind], &g_tcrt_ctx.dest_ip, NULL, 0);
    if (ret) {
        fprintf(stderr, "%s: %s\n", argv[0], gai_strerror(ret));
        exit(EXIT_FAILURE);
    }

    // Get packetlen if exists
    if (optind + 1 < argc) {
        g_tcrt_ctx.pack_len = atoi(argv[optind + 1]);
    }

    for (int i = optind; i < argc; i++) {
        printf("%s\n", argv[i]);
    }

    initialize_sockets();
    initialize_signals();
}

static void initialize_sockets() {
    int sock = -1, setsock = -1;

    if (g_tcrt_ctx.flags & TRCRT_ICMP)
        sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    else if (g_tcrt_ctx.flags & TRCRT_TCP)
        ;
    else
        sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);

    if (sock < 0) {
        perror("cannot create socket");
        exit(EXIT_FAILURE);
    }

    if (g_tcrt_ctx.flags & TRCRT_ICMP)
        setsock = setsockopt(sock, IPPROTO_IP, IP_HDRINCL, (int[1]){1}, sizeof(int));
    else if (g_tcrt_ctx.flags & TRCRT_TCP)
        ;
    else
        ;

    if (setsock < 0) {
        perror("cannot set sock option");
        close(sock);
        exit(EXIT_FAILURE);
    }

    g_tcrt_ctx.sock = sock;
}

void f(int a){
    printf("alarm\n");
}

static void initialize_signals() {
    struct sigaction sa;
    memset(&sa, 0x0, sizeof sa);
    sa.sa_handler = f;
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
    "[ -s src_addr ] "
    "[ -t tos ] "
    "host "
    "[ packetlen ]\n"
    "Options:\n"
    "  -f first_ttl  --first=first_ttl\n"
    "                              Start from the first_ttl hop (instead from 1)\n"
    "  -I  --icmp                  Use ICMP ECHO for tracerouting\n"
    "  -T  --tcp                   Use TCP SYN for tracerouting (default port is 80)\n"
    "  -i device  --interface=device\n"
    "                              Specify a network interface to operate with\n"
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
    "  -t tos  --tos=tos           Set the TOS (IPv4 type of service) or TC (IPv6\n"
    "                              traffic class) value for outgoing packets\n"
    "  -s src_addr  --source=src_addr\n"
    "                              Use source src_addr for outgoing packets\n"
    "  -z sendwait  --sendwait=sendwait\n"
    "                              Minimal time interval between probes (default 0).\n"
    "                              If the value is more than 10, then it specifies a\n"
    "                              number in milliseconds, else it is a number of\n"
    "                              seconds (float point values allowed too)\n"
    "  --sport=num                 Use source port num for outgoing packets. Implies\n"
    "                              `-N 1'\n"
    "  -V  --version               Print version info and exit\n"
    "  -h  --help                  Read this help and exit\n"
    "\n"
    "Arguments:\n"
    "+     host          The host to traceroute to\n"
    "      packetlen     The full packet length (default is the length of an IP\n"
    "                    header plus 40). Can be ignored or increased to a minimal\n"
    "                    allowed value", bin_name);
}

static void set_default_args() {
    memset(&g_tcrt_ctx, 0x0, sizeof g_tcrt_ctx);

    g_tcrt_ctx.dest_port = DEFAULT_START_PORT;
    g_tcrt_ctx.send_wait = 0;
    g_tcrt_ctx.max_ttl = 255; /* Max possible ttl */
    g_tcrt_ctx.current_ttl = 1;
}

static void dump_version() {
    printf("%s built on %s at %s\n", "ft_tracroute v0.0.1", __DATE__, __TIME__);
}
