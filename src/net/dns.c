#include "net/dns.h"
#include "log.h"
#include "util/strutil.h"

#include <sys/socket.h>
#include <netdb.h>
#include <string.h>

int ri_dns_reverse_ipv4(struct in_addr addr, char *buf, size_t buflen)
{
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_addr = addr;

    int rc = getnameinfo((struct sockaddr *)&sa, sizeof(sa),
                         buf, (socklen_t)buflen,
                         NULL, 0, NI_NAMEREQD);
    return (rc == 0) ? 0 : -1;
}

int ri_dns_resolve_all(ri_graph_t *g)
{
    int count = 0;
    for (int i = 0; i < g->host_count; i++) {
        ri_host_t *h = &g->hosts[i];
        if (!h->has_ipv4) continue;
        if (!ri_str_empty(h->dns_name)) continue;

        char name[RI_HOSTNAME_LEN];
        if (ri_dns_reverse_ipv4(h->ipv4, name, sizeof(name)) == 0) {
            ri_strlcpy(h->dns_name, name, sizeof(h->dns_name));
            LOG_DEBUG("DNS: %s -> %s", ri_host_ipv4_str(h), name);
            count++;
        }
    }
    return count;
}
