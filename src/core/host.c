#include "core/host.h"
#include "util/strutil.h"
#include <string.h>
#include <stdio.h>

void ri_host_init(ri_host_t *h)
{
    memset(h, 0, sizeof(*h));
    h->id = -1;
    h->type = RI_HOST_LAN;
    h->hop_distance = -1;
    h->rtt_ms = -1.0;
}

int ri_host_set_ipv4(ri_host_t *h, const char *addr)
{
    if (inet_pton(AF_INET, addr, &h->ipv4) != 1) {
        return -1;
    }
    h->has_ipv4 = 1;
    return 0;
}

int ri_host_set_ipv4_addr(ri_host_t *h, struct in_addr addr)
{
    h->ipv4 = addr;
    h->has_ipv4 = 1;
    return 0;
}

int ri_host_set_ipv6(ri_host_t *h, const char *addr)
{
    if (inet_pton(AF_INET6, addr, &h->ipv6) != 1) {
        return -1;
    }
    h->has_ipv6 = 1;
    return 0;
}

int ri_host_set_mac(ri_host_t *h, const unsigned char *mac)
{
    memcpy(h->mac, mac, RI_MAC_LEN);
    h->has_mac = 1;
    return 0;
}

void ri_host_add_ipv4(ri_host_t *h, struct in_addr addr)
{
    /* If primary slot is empty, use it */
    if (!h->has_ipv4) {
        h->ipv4 = addr;
        h->has_ipv4 = 1;
        return;
    }
    /* Skip if duplicate of primary */
    if (h->ipv4.s_addr == addr.s_addr) return;
    /* Skip if duplicate of existing secondary */
    for (int i = 0; i < h->ipv4_count; i++) {
        if (h->ipv4_addrs[i].s_addr == addr.s_addr) return;
    }
    /* Add to secondary list */
    if (h->ipv4_count < RI_MAX_ADDRS)
        h->ipv4_addrs[h->ipv4_count++] = addr;
}

void ri_host_add_ipv6(ri_host_t *h, const struct in6_addr *addr)
{
    if (!h->has_ipv6) {
        memcpy(&h->ipv6, addr, sizeof(struct in6_addr));
        h->has_ipv6 = 1;
        return;
    }
    if (memcmp(&h->ipv6, addr, sizeof(struct in6_addr)) == 0) return;
    for (int i = 0; i < h->ipv6_count; i++) {
        if (memcmp(&h->ipv6_addrs[i], addr, sizeof(struct in6_addr)) == 0) return;
    }
    if (h->ipv6_count < RI_MAX_ADDRS)
        memcpy(&h->ipv6_addrs[h->ipv6_count++], addr, sizeof(struct in6_addr));
}

void ri_host_add_iface(ri_host_t *h, const char *iface)
{
    if (!iface || iface[0] == '\0') return;

    /* Check if already listed */
    if (h->interfaces[0] != '\0') {
        /* Search for exact match in comma-separated list */
        const char *p = h->interfaces;
        size_t ilen = strlen(iface);
        while (*p) {
            if (strncmp(p, iface, ilen) == 0 &&
                (p[ilen] == ',' || p[ilen] == '\0'))
                return; /* already present */
            p = strchr(p, ',');
            if (!p) break;
            p++; /* skip comma */
        }
        ri_strlcat(h->interfaces, ",", sizeof(h->interfaces));
    }
    ri_strlcat(h->interfaces, iface, sizeof(h->interfaces));

    /* Set primary iface_name if empty */
    if (h->iface_name[0] == '\0')
        ri_strlcpy(h->iface_name, iface, sizeof(h->iface_name));
}

/* Check if a name looks like an OS interface name (en0, utun3, bridge100, etc.) */
static int is_iface_name(const char *s)
{
    if (!s || !*s) return 0;
    /* Interface names: short alphanumeric, typically letters then digits */
    size_t len = strlen(s);
    if (len > 16) return 0;
    /* Known prefixes */
    static const char *prefixes[] = {
        "en", "eth", "wlan", "utun", "lo", "bridge", "feth",
        "awdl", "llw", "ap", "ipsec", "gif", "stf", "XHC",
        "anpi", "vmnet", "veth", "docker", "br-", NULL
    };
    for (const char **p = prefixes; *p; p++) {
        size_t plen = strlen(*p);
        if (len >= plen && strncmp(s, *p, plen) == 0)
            return 1;
    }
    return 0;
}

void ri_host_compute_display_name(ri_host_t *h)
{
    /* Priority: dns_name > hostname (if not iface) > mdns > IPv4 > IPv6 */
    if (!ri_str_empty(h->dns_name)) {
        ri_strlcpy(h->display_name, h->dns_name, sizeof(h->display_name));
    } else if (!ri_str_empty(h->hostname) && !is_iface_name(h->hostname)) {
        ri_strlcpy(h->display_name, h->hostname, sizeof(h->display_name));
    } else if (!ri_str_empty(h->mdns_name)) {
        ri_strlcpy(h->display_name, h->mdns_name, sizeof(h->display_name));
        ri_str_unescape_mdns(h->display_name);
    } else if (h->has_ipv4) {
        inet_ntop(AF_INET, &h->ipv4, h->display_name,
                  sizeof(h->display_name));
    } else if (h->has_ipv6) {
        inet_ntop(AF_INET6, &h->ipv6, h->display_name,
                  sizeof(h->display_name));
    } else {
        ri_strlcpy(h->display_name, "(unknown)", sizeof(h->display_name));
    }
}

const char *ri_host_ipv4_str(const ri_host_t *h)
{
    static char buf[RI_IPV4_STR_LEN];
    if (!h->has_ipv4) return "(none)";
    inet_ntop(AF_INET, &h->ipv4, buf, sizeof(buf));
    return buf;
}

const char *ri_host_type_str(ri_host_type_t type)
{
    switch (type) {
        case RI_HOST_LOCAL:   return "local";
        case RI_HOST_LAN:     return "lan";
        case RI_HOST_GATEWAY: return "gateway";
        case RI_HOST_REMOTE:  return "remote";
        case RI_HOST_TARGET:  return "target";
    }
    return "unknown";
}
