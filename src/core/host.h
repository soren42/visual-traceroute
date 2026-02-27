#ifndef RI_HOST_H
#define RI_HOST_H

#include "util/platform.h"
#include <netinet/in.h>

typedef enum {
    RI_HOST_LOCAL   = 0,
    RI_HOST_LAN     = 1,
    RI_HOST_GATEWAY = 2,
    RI_HOST_REMOTE  = 3,
    RI_HOST_TARGET  = 4
} ri_host_type_t;

#define RI_MAX_ADDRS  8  /* max secondary addresses per host */
#define RI_IFACES_LEN 256 /* comma-separated interface list */

typedef struct {
    int                  id;
    struct in_addr       ipv4;
    struct in6_addr      ipv6;
    unsigned char        mac[RI_MAC_LEN];
    char                 hostname[RI_HOSTNAME_LEN];
    char                 mdns_name[RI_HOSTNAME_LEN];
    char                 dns_name[RI_HOSTNAME_LEN];
    char                 display_name[RI_HOSTNAME_LEN];
    char                 iface_name[32];
    char                 interfaces[RI_IFACES_LEN]; /* comma-separated list */
    ri_host_type_t       type;
    int                  hop_distance;
    double               rtt_ms;
    int                  has_ipv4;
    int                  has_ipv6;
    int                  has_mac;
    /* Secondary addresses (for consolidated hosts) */
    struct in_addr       ipv4_addrs[RI_MAX_ADDRS];
    int                  ipv4_count;
    struct in6_addr      ipv6_addrs[RI_MAX_ADDRS];
    int                  ipv6_count;
    /* Layout coordinates */
    double               x, y, z;
} ri_host_t;

/* Initialize a host with defaults */
void ri_host_init(ri_host_t *h);

/* Set IPv4 address, returns 0 on success */
int ri_host_set_ipv4(ri_host_t *h, const char *addr);
int ri_host_set_ipv4_addr(ri_host_t *h, struct in_addr addr);

/* Set IPv6 address */
int ri_host_set_ipv6(ri_host_t *h, const char *addr);

/* Set MAC address */
int ri_host_set_mac(ri_host_t *h, const unsigned char *mac);

/* Add a secondary IPv4 address */
void ri_host_add_ipv4(ri_host_t *h, struct in_addr addr);

/* Add a secondary IPv6 address */
void ri_host_add_ipv6(ri_host_t *h, const struct in6_addr *addr);

/* Append an interface name to the interfaces list */
void ri_host_add_iface(ri_host_t *h, const char *iface);

/* Compute display_name: dns_name > hostname > mdns > IP > "(unknown)" */
void ri_host_compute_display_name(ri_host_t *h);

/* Get IPv4 as string, returns static buffer */
const char *ri_host_ipv4_str(const ri_host_t *h);

/* Get type as string */
const char *ri_host_type_str(ri_host_type_t type);

#endif /* RI_HOST_H */
