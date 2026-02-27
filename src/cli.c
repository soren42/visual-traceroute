#include "cli.h"
#include "log.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

void ri_cli_defaults(ri_config_t *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->max_depth = 1;
    cfg->output_flags = RI_OUT_JSON; /* default output */
    strcpy(cfg->file_base, "network");
}

static unsigned int parse_output_formats(const char *str)
{
    unsigned int flags = 0;
    char buf[256];
    strncpy(buf, str, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *tok = strtok(buf, ",");
    while (tok) {
        while (*tok == ' ') tok++;
        if (strcmp(tok, "json") == 0)        flags |= RI_OUT_JSON;
        else if (strcmp(tok, "curses") == 0)  flags |= RI_OUT_CURSES;
        else if (strcmp(tok, "png") == 0)     flags |= RI_OUT_PNG;
        else if (strcmp(tok, "mp4") == 0)     flags |= RI_OUT_MP4;
        else if (strcmp(tok, "html") == 0)    flags |= RI_OUT_HTML;
        else {
            fprintf(stderr, "Unknown output format: %s\n", tok);
            return 0;
        }
        tok = strtok(NULL, ",");
    }
    return flags;
}

void ri_cli_usage(const char *prog)
{
    fprintf(stderr,
        "Usage: %s [OPTIONS]\n"
        "\n"
        "Visual Traceroute - Network Discovery & 3D Visualization\n"
        "\n"
        "Options:\n"
        "  -t, --target HOST    Deep traceroute toward HOST\n"
        "  -d, --depth N        Max hops beyond gateway (default: 1)\n"
        "  -v                   Increase verbosity (-v to -vvvvv)\n"
        "  -o, --output FMT     Output formats: json,curses,png,mp4,html\n"
        "  -f, --file PATH      Output filename base (default: network)\n"
        "  -4                   IPv4 only\n"
        "  -6                   IPv6 only\n"
        "  --no-mdns            Disable mDNS discovery\n"
        "  --no-arp             Disable ARP cache reading\n"
        "  --subnet-scan        Enable ping sweep of local subnets\n"
        "  --hop-scan           Probe /24 around each traceroute hop\n"
        "  -h, --help           Show this help\n"
        "  --version            Show version\n"
        "\n", prog);
}

int ri_cli_parse(ri_config_t *cfg, int argc, char **argv)
{
    ri_cli_defaults(cfg);

    static struct option long_opts[] = {
        {"target",      required_argument, NULL, 't'},
        {"depth",       required_argument, NULL, 'd'},
        {"output",      required_argument, NULL, 'o'},
        {"file",        required_argument, NULL, 'f'},
        {"no-mdns",     no_argument,       NULL, 'M'},
        {"no-arp",      no_argument,       NULL, 'A'},
        {"subnet-scan", no_argument,       NULL, 'S'},
        {"hop-scan",    no_argument,       NULL, 'H'},
        {"help",        no_argument,       NULL, 'h'},
        {"version",     no_argument,       NULL, 'V'},
        {NULL, 0, NULL, 0}
    };

    int opt;
    int v_count = 0;

    /* Reset getopt for testability */
    optind = 1;

    while ((opt = getopt_long(argc, argv, "t:d:vo:f:46h", long_opts, NULL)) != -1) {
        switch (opt) {
        case 't':
            strncpy(cfg->target, optarg, sizeof(cfg->target) - 1);
            cfg->has_target = 1;
            break;
        case 'd':
            cfg->max_depth = atoi(optarg);
            if (cfg->max_depth < 0) cfg->max_depth = 0;
            break;
        case 'v':
            v_count++;
            break;
        case 'o': {
            unsigned int flags = parse_output_formats(optarg);
            if (flags == 0) return -1;
            cfg->output_flags = flags;
            break;
        }
        case 'f':
            strncpy(cfg->file_base, optarg, sizeof(cfg->file_base) - 1);
            break;
        case '4':
            cfg->ipv4_only = 1;
            cfg->ipv6_only = 0;
            break;
        case '6':
            cfg->ipv6_only = 1;
            cfg->ipv4_only = 0;
            break;
        case 'M':
            cfg->no_mdns = 1;
            break;
        case 'A':
            cfg->no_arp = 1;
            break;
        case 'S':
            cfg->subnet_scan = 1;
            break;
        case 'H':
            cfg->hop_scan = 1;
            break;
        case 'h':
            ri_cli_usage(argv[0]);
            exit(0);
        case 'V':
            printf("visual-traceroute %s\n", RI_VERSION);
            exit(0);
        default:
            return -1;
        }
    }

    cfg->verbosity = v_count;
    if (cfg->verbosity > 5) cfg->verbosity = 5;

    return 0;
}
