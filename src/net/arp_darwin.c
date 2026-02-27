#include "config.h"
#ifdef RI_DARWIN

/* System headers first for routing structures */
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>

/* Project headers */
#include "net/arp.h"
#include "log.h"
#include "util/alloc.h"
#include "util/strutil.h"

#define SA_SIZE(sa) \
    ((sa)->sa_len ? (((sa)->sa_len + sizeof(long) - 1) & ~(sizeof(long) - 1)) \
                  : sizeof(long))

static void extract_addrs(struct rt_msghdr *rtm, struct sockaddr *addrs[],
                          int max)
{
    memset(addrs, 0, (size_t)max * sizeof(struct sockaddr *));
    struct sockaddr *sa = (struct sockaddr *)(rtm + 1);
    for (int i = 0; i < max; i++) {
        if (rtm->rtm_addrs & (1 << i)) {
            addrs[i] = sa;
            sa = (struct sockaddr *)((char *)sa + SA_SIZE(sa));
        }
    }
}

static int mac_is_zero(const unsigned char *mac)
{
    for (int i = 0; i < 6; i++)
        if (mac[i] != 0) return 0;
    return 1;
}

int ri_arp_read(ri_graph_t *g)
{
    int mib[6] = { CTL_NET, PF_ROUTE, 0, AF_INET, NET_RT_FLAGS, RTF_LLINFO };
    size_t buflen = 0;

    if (sysctl(mib, 6, NULL, &buflen, NULL, 0) < 0) {
        LOG_ERROR("sysctl ARP size: %s", strerror(errno));
        return -1;
    }
    if (buflen == 0) return 0;

    char *buf = ri_malloc(buflen);
    if (sysctl(mib, 6, buf, &buflen, NULL, 0) < 0) {
        LOG_ERROR("sysctl ARP read: %s", strerror(errno));
        ri_free(buf);
        return -1;
    }

    /* Find first local host */
    int local_id = -1;
    for (int i = 0; i < g->host_count; i++) {
        if (g->hosts[i].type == RI_HOST_LOCAL) {
            local_id = i;
            break;
        }
    }

    int count = 0;
    char *lim = buf + buflen;
    char *next = buf;

    while (next < lim) {
        struct rt_msghdr *rtm = (struct rt_msghdr *)next;
        if (rtm->rtm_msglen == 0) break;
        next += rtm->rtm_msglen;

        struct sockaddr *addrs[RTAX_MAX];
        extract_addrs(rtm, addrs, RTAX_MAX);

        if (!addrs[RTAX_DST] || addrs[RTAX_DST]->sa_family != AF_INET)
            continue;
        if (!addrs[RTAX_GATEWAY] || addrs[RTAX_GATEWAY]->sa_family != AF_LINK)
            continue;

        struct sockaddr_in *sin = (struct sockaddr_in *)addrs[RTAX_DST];
        struct sockaddr_dl *sdl = (struct sockaddr_dl *)addrs[RTAX_GATEWAY];

        if (sdl->sdl_alen != 6) continue;
        unsigned char *mac = (unsigned char *)LLADDR(sdl);
        if (mac_is_zero(mac)) continue;

        struct in_addr ip = sin->sin_addr;
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
            if (sdl->sdl_index > 0) {
                char ifname[32];
                if (if_indextoname(sdl->sdl_index, ifname))
                    ri_strlcpy(h.iface_name, ifname, sizeof(h.iface_name));
            }
            host_id = ri_graph_add_host(g, &h);
        }

        if (local_id >= 0 && !ri_graph_has_edge(g, local_id, host_id))
            ri_graph_add_edge(g, local_id, host_id, 1.0, RI_EDGE_LAN);

        count++;
    }

    ri_free(buf);
    LOG_INFO("ARP: %d entries", count);
    return count;
}

#endif /* RI_DARWIN */
