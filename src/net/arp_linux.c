#include "config.h"
#ifdef RI_LINUX

#include "net/arp.h"
#include "log.h"
#include "util/alloc.h"
#include "util/strutil.h"

#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

int ri_arp_read(ri_graph_t *g)
{
    FILE *fp = fopen("/proc/net/arp", "r");
    if (!fp) {
        LOG_ERROR("Cannot open /proc/net/arp");
        return -1;
    }

    int local_id = -1;
    for (int i = 0; i < g->host_count; i++) {
        if (g->hosts[i].type == RI_HOST_LOCAL) {
            local_id = i;
            break;
        }
    }

    char line[256];
    int count = 0;
    /* Skip header */
    if (!fgets(line, sizeof(line), fp)) { fclose(fp); return 0; }

    while (fgets(line, sizeof(line), fp)) {
        char ip_str[32], mac_str[32], dev[32];
        unsigned int hw_type, flags;
        /* Format: IP HW_type Flags HW_addr Mask Device */
        if (sscanf(line, "%31s 0x%x 0x%x %31s %*s %31s",
                   ip_str, &hw_type, &flags, mac_str, dev) < 5)
            continue;

        /* Skip incomplete entries (flags & 0x02 means complete) */
        if (!(flags & 0x02)) continue;

        struct in_addr ip;
        if (inet_pton(AF_INET, ip_str, &ip) != 1) continue;

        unsigned char mac[6];
        if (ri_str_to_mac(mac_str, mac) != 0) continue;

        int host_id = ri_graph_find_by_ipv4(g, ip);
        if (host_id >= 0) {
            if (!g->hosts[host_id].has_mac)
                ri_host_set_mac(&g->hosts[host_id], mac);
        } else {
            ri_host_t h;
            ri_host_init(&h);
            ri_host_set_ipv4_addr(&h, ip);
            ri_host_set_mac(&h, mac);
            h.type = RI_HOST_LAN;
            ri_strlcpy(h.iface_name, dev, sizeof(h.iface_name));
            host_id = ri_graph_add_host(g, &h);
        }

        if (local_id >= 0 && !ri_graph_has_edge(g, local_id, host_id))
            ri_graph_add_edge(g, local_id, host_id, 1.0, RI_EDGE_LAN);

        count++;
    }

    fclose(fp);
    LOG_INFO("ARP: %d entries", count);
    return count;
}

#endif /* RI_LINUX */
