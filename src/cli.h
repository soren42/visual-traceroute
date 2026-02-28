#ifndef RI_CLI_H
#define RI_CLI_H

#include <netinet/in.h>

/* Output format flags (bitmask) */
#define RI_OUT_JSON    (1 << 0)
#define RI_OUT_CURSES  (1 << 1)
#define RI_OUT_PNG     (1 << 2)
#define RI_OUT_MP4     (1 << 3)
#define RI_OUT_HTML    (1 << 4)

typedef struct {
    char             target[256];
    int              max_depth;
    int              verbosity;
    unsigned int     output_flags;
    char             file_base[256];
    int              ipv4_only;
    int              ipv6_only;
    int              no_mdns;
    int              no_arp;
    int              subnet_scan;
    int              hop_scan;
    int              web_mode;
    int              has_target;
} ri_config_t;

/* Parse command-line arguments into config. Returns 0 on success, -1 on error. */
int ri_cli_parse(ri_config_t *cfg, int argc, char **argv);

/* Initialize config with defaults */
void ri_cli_defaults(ri_config_t *cfg);

/* Print usage text */
void ri_cli_usage(const char *prog);

#endif /* RI_CLI_H */
