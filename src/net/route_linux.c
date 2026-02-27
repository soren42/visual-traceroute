#include "config.h"
#ifdef RI_LINUX

#include "net/ri_route.h"
#include "log.h"
#include "util/alloc.h"
#include "util/strutil.h"

#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

int ri_route_read(ri_graph_t *g)
{
    FILE *fp = fopen("/proc/net/route", "r");
    if (!fp) {
        LOG_ERROR("Cannot open /proc/net/route");
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
        char iface[32];
        unsigned int dest, gateway, flags;
        if (sscanf(line, "%31s %X %X %X", iface, &dest, &gateway, &flags) < 4)
            continue;

        /* RTF_GATEWAY = 0x0002 */
        if (!(flags & 0x0002)) continue;
        if (gateway == 0) continue;

        struct in_addr gw_addr;
        gw_addr.s_addr = gateway; /* already in network byte order */

        int gw_id = ri_graph_find_by_ipv4(g, gw_addr);
        if (gw_id < 0) {
            ri_host_t h;
            ri_host_init(&h);
            ri_host_set_ipv4_addr(&h, gw_addr);
            h.type = RI_HOST_GATEWAY;
            h.hop_distance = 1;
            ri_strlcpy(h.iface_name, iface, sizeof(h.iface_name));
            gw_id = ri_graph_add_host(g, &h);
            count++;
        }

        if (local_id >= 0 && !ri_graph_has_edge(g, local_id, gw_id))
            ri_graph_add_edge(g, local_id, gw_id, 1.0, RI_EDGE_GATEWAY);
    }

    fclose(fp);
    LOG_INFO("Found %d gateway(s)", count);
    return count;
}

#endif /* RI_LINUX */
