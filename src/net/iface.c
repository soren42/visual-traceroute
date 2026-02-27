#include "net/iface.h"
#include "log.h"
#include "util/strutil.h"

#include <ifaddrs.h>
#include <string.h>
#include <net/if.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifdef RI_DARWIN
#include <net/if_dl.h>
#endif
#ifdef RI_LINUX
#include <linux/if_packet.h>
#endif

int ri_iface_enumerate(ri_graph_t *g)
{
    struct ifaddrs *ifap, *ifa;
    if (getifaddrs(&ifap) != 0) {
        LOG_ERROR("getifaddrs failed");
        return -1;
    }

    /* Create a single LOCAL host for this machine */
    int local_id = -1;
    for (int i = 0; i < g->host_count; i++) {
        if (g->hosts[i].type == RI_HOST_LOCAL) {
            local_id = i;
            break;
        }
    }
    if (local_id < 0) {
        ri_host_t h;
        ri_host_init(&h);
        h.type = RI_HOST_LOCAL;
        h.hop_distance = 0;
        h.rtt_ms = 0;
        local_id = ri_graph_add_host(g, &h);
    }

    ri_host_t *local = &g->hosts[local_id];
    int iface_count = 0;

    /* First pass: collect MACs and interface names */
    for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) continue;
        if (ifa->ifa_flags & IFF_LOOPBACK) continue;
        if (!(ifa->ifa_flags & IFF_UP)) continue;

#ifdef RI_DARWIN
        if (ifa->ifa_addr->sa_family == AF_LINK) {
            struct sockaddr_dl *sdl = (struct sockaddr_dl *)ifa->ifa_addr;
            if (sdl->sdl_alen == 6) {
                unsigned char *mac = (unsigned char *)LLADDR(sdl);
                /* Use first non-zero MAC as primary */
                int is_zero = 1;
                for (int j = 0; j < 6; j++)
                    if (mac[j] != 0) { is_zero = 0; break; }
                if (!is_zero && !local->has_mac)
                    ri_host_set_mac(local, mac);
            }
        }
#endif
#ifdef RI_LINUX
        if (ifa->ifa_addr->sa_family == AF_PACKET) {
            struct sockaddr_ll *sll = (struct sockaddr_ll *)ifa->ifa_addr;
            if (sll->sll_halen == 6) {
                int is_zero = 1;
                for (int j = 0; j < 6; j++)
                    if (sll->sll_addr[j] != 0) { is_zero = 0; break; }
                if (!is_zero && !local->has_mac)
                    ri_host_set_mac(local, sll->sll_addr);
            }
        }
#endif
    }

    /* Second pass: collect IPs */
    for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) continue;
        if (ifa->ifa_flags & IFF_LOOPBACK) continue;
        if (!(ifa->ifa_flags & IFF_UP)) continue;

        int family = ifa->ifa_addr->sa_family;

        if (family == AF_INET) {
            struct sockaddr_in *sin = (struct sockaddr_in *)ifa->ifa_addr;
            ri_host_add_ipv4(local, sin->sin_addr);
            ri_host_add_iface(local, ifa->ifa_name);
            iface_count++;

            char abuf[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &sin->sin_addr, abuf, sizeof(abuf));
            LOG_DEBUG("Interface %s: %s (IPv4)", ifa->ifa_name, abuf);
        }
        else if (family == AF_INET6) {
            struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)ifa->ifa_addr;
            ri_host_add_ipv6(local, &sin6->sin6_addr);
            ri_host_add_iface(local, ifa->ifa_name);
            iface_count++;

            char abuf[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, &sin6->sin6_addr, abuf, sizeof(abuf));
            LOG_DEBUG("Interface %s: %s (IPv6)", ifa->ifa_name, abuf);
        }
    }

    freeifaddrs(ifap);
    return iface_count;
}
