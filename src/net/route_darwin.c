#include "config.h"
#ifdef RI_DARWIN

/* Include system routing headers first, before anything redefines macros */
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/socket.h>
#include <net/route.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

/* Now include project headers */
#include "net/ri_route.h"
#include "log.h"
#include "util/alloc.h"

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

int ri_route_read(ri_graph_t *g)
{
    int mib[6] = { CTL_NET, PF_ROUTE, 0, AF_INET, NET_RT_FLAGS, RTF_GATEWAY };
    size_t buflen = 0;

    if (sysctl(mib, 6, NULL, &buflen, NULL, 0) < 0) {
        LOG_ERROR("sysctl route size: %s", strerror(errno));
        return -1;
    }
    if (buflen == 0) return 0;

    char *buf = ri_malloc(buflen);
    if (sysctl(mib, 6, buf, &buflen, NULL, 0) < 0) {
        LOG_ERROR("sysctl route read: %s", strerror(errno));
        ri_free(buf);
        return -1;
    }

    /* Find first local host ID */
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

        if (!(rtm->rtm_flags & RTF_GATEWAY)) continue;

        struct sockaddr *addrs[RTAX_MAX];
        extract_addrs(rtm, addrs, RTAX_MAX);

        if (!addrs[RTAX_GATEWAY] || addrs[RTAX_GATEWAY]->sa_family != AF_INET)
            continue;

        struct sockaddr_in *sin = (struct sockaddr_in *)addrs[RTAX_GATEWAY];
        struct in_addr gw_addr = sin->sin_addr;

        int gw_id = ri_graph_find_by_ipv4(g, gw_addr);
        if (gw_id < 0) {
            ri_host_t h;
            ri_host_init(&h);
            ri_host_set_ipv4_addr(&h, gw_addr);
            h.type = RI_HOST_GATEWAY;
            h.hop_distance = 1;
            gw_id = ri_graph_add_host(g, &h);
            count++;

            char abuf[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &gw_addr, abuf, sizeof(abuf));
            LOG_DEBUG("Gateway: %s", abuf);
        }

        if (local_id >= 0 && !ri_graph_has_edge(g, local_id, gw_id)) {
            ri_graph_add_edge(g, local_id, gw_id, 1.0, RI_EDGE_GATEWAY);
        }
    }

    ri_free(buf);
    LOG_INFO("Found %d gateway(s)", count);
    return count;
}

#endif /* RI_DARWIN */
