#include "config.h"
#if defined(RI_DARWIN) && defined(HAVE_DNS_SD)

#include "net/mdns.h"
#include "log.h"
#include "util/strutil.h"

#include <dns_sd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>

static ri_graph_t *g_browse_graph = NULL;
static int g_mdns_assigned = 0;

static void resolve_callback(DNSServiceRef ref, DNSServiceFlags flags,
                             uint32_t iface, DNSServiceErrorType err,
                             const char *fullname, const char *hosttarget,
                             uint16_t port, uint16_t txt_len,
                             const unsigned char *txt, void *ctx)
{
    (void)ref; (void)flags; (void)iface; (void)port;
    (void)txt_len; (void)txt; (void)ctx;

    if (err != kDNSServiceErr_NoError) return;

    LOG_DEBUG("mDNS resolve: %s -> %s", fullname, hosttarget);

    ri_graph_t *g = g_browse_graph;
    if (!g) return;

    /* Resolve hosttarget to an IP address to match against graph */
    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(hosttarget, NULL, &hints, &res) == 0 && res) {
        struct sockaddr_in *sin = (struct sockaddr_in *)res->ai_addr;
        int id = ri_graph_find_by_ipv4(g, sin->sin_addr);
        if (id >= 0 && ri_str_empty(g->hosts[id].mdns_name)) {
            ri_strlcpy(g->hosts[id].mdns_name, fullname,
                       sizeof(g->hosts[id].mdns_name));
            LOG_DEBUG("mDNS: assigned %s to host %d (%s)",
                      fullname, id, hosttarget);
            g_mdns_assigned++;
        }
        freeaddrinfo(res);
    }
}

static void browse_callback(DNSServiceRef ref, DNSServiceFlags flags,
                            uint32_t iface, DNSServiceErrorType err,
                            const char *name, const char *regtype,
                            const char *domain, void *ctx)
{
    (void)ref; (void)ctx;

    if (err != kDNSServiceErr_NoError) return;
    if (!(flags & kDNSServiceFlagsAdd)) return;

    LOG_DEBUG("mDNS browse: %s.%s%s on iface %u", name, regtype, domain, iface);

    /* Resolve this service */
    DNSServiceRef resolve_ref;
    DNSServiceErrorType resolve_err = DNSServiceResolve(
        &resolve_ref, 0, iface, name, regtype, domain,
        resolve_callback, NULL);

    if (resolve_err == kDNSServiceErr_NoError) {
        /* Process with short timeout */
        int fd = DNSServiceRefSockFD(resolve_ref);
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };
        if (select(fd + 1, &fds, NULL, NULL, &tv) > 0) {
            DNSServiceProcessResult(resolve_ref);
        }
        DNSServiceRefDeallocate(resolve_ref);
    }
}

int ri_mdns_browse(ri_graph_t *g, int timeout_ms)
{
    g_browse_graph = g;
    g_mdns_assigned = 0;

    DNSServiceRef browse_ref;
    DNSServiceErrorType err = DNSServiceBrowse(
        &browse_ref, 0, 0, "_http._tcp", NULL,
        browse_callback, NULL);

    if (err != kDNSServiceErr_NoError) {
        LOG_WARN("mDNS browse failed: %d", err);
        return -1;
    }

    int fd = DNSServiceRefSockFD(browse_ref);
    int elapsed = 0;
    int step = 100; /* ms per select iteration */

    while (elapsed < timeout_ms) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        struct timeval tv;
        tv.tv_sec = step / 1000;
        tv.tv_usec = (step % 1000) * 1000;

        int rc = select(fd + 1, &fds, NULL, NULL, &tv);
        if (rc > 0) {
            DNSServiceProcessResult(browse_ref);
        }
        elapsed += step;
    }

    DNSServiceRefDeallocate(browse_ref);
    g_browse_graph = NULL;
    return g_mdns_assigned;
}

#elif defined(RI_DARWIN)
/* dns_sd not available - stub */
#include "net/mdns.h"
#include "log.h"

int ri_mdns_browse(ri_graph_t *g, int timeout_ms)
{
    (void)g; (void)timeout_ms;
    LOG_DEBUG("mDNS: dns_sd not available");
    return 0;
}
#endif
