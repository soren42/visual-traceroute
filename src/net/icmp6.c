#include "net/icmp6.h"
#include "core/graph.h"
#include "log.h"
#include "util/strutil.h"

#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/icmp6.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <net/if.h>

static unsigned short g_seq6 = 0;

static double now_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

double ri_icmp6_ping(const struct in6_addr *target, int timeout_ms)
{
    int sock = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
    if (sock < 0) {
        LOG_ERROR("ICMPv6 socket: %s", strerror(errno));
        return -1.0;
    }

    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct sockaddr_in6 dst;
    memset(&dst, 0, sizeof(dst));
    dst.sin6_family = AF_INET6;
    dst.sin6_addr = *target;

    struct icmp6_hdr pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.icmp6_type = ICMP6_ECHO_REQUEST;
    pkt.icmp6_code = 0;
    pkt.icmp6_id = htons(getpid() & 0xFFFF);
    pkt.icmp6_seq = htons(++g_seq6);
    /* Kernel computes ICMPv6 checksum */

    double t0 = now_ms();
    if (sendto(sock, &pkt, sizeof(pkt), 0,
               (struct sockaddr *)&dst, sizeof(dst)) < 0) {
        close(sock);
        return -1.0;
    }

    unsigned char buf[512];
    ssize_t n = recv(sock, buf, sizeof(buf), 0);
    double t1 = now_ms();
    close(sock);

    if (n < (ssize_t)sizeof(struct icmp6_hdr)) return -1.0;

    struct icmp6_hdr *reply = (struct icmp6_hdr *)buf;
    if (reply->icmp6_type != ICMP6_ECHO_REPLY) return -1.0;

    return t1 - t0;
}

int ri_icmp6_probe(const struct in6_addr *target, int hop_limit,
                   int timeout_ms, struct in6_addr *reply_addr,
                   double *rtt_ms)
{
    if (!reply_addr || !rtt_ms) return -1;

    int sock = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
    if (sock < 0) return -1;

    setsockopt(sock, IPPROTO_IPV6, IPV6_UNICAST_HOPS,
               &hop_limit, sizeof(hop_limit));

    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct sockaddr_in6 dst;
    memset(&dst, 0, sizeof(dst));
    dst.sin6_family = AF_INET6;
    dst.sin6_addr = *target;

    struct icmp6_hdr pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.icmp6_type = ICMP6_ECHO_REQUEST;
    pkt.icmp6_code = 0;
    pkt.icmp6_id = htons(getpid() & 0xFFFF);
    pkt.icmp6_seq = htons(++g_seq6);

    double t0 = now_ms();
    if (sendto(sock, &pkt, sizeof(pkt), 0,
               (struct sockaddr *)&dst, sizeof(dst)) < 0) {
        close(sock);
        return -1;
    }

    unsigned char buf[512];
    struct sockaddr_in6 from;
    socklen_t fromlen = sizeof(from);
    ssize_t n = recvfrom(sock, buf, sizeof(buf), 0,
                         (struct sockaddr *)&from, &fromlen);
    double t1 = now_ms();

    if (n < 0) { close(sock); return -1; }
    *rtt_ms = t1 - t0;
    *reply_addr = from.sin6_addr;

    struct icmp6_hdr *reply = (struct icmp6_hdr *)buf;
    int result;
    if (reply->icmp6_type == ICMP6_ECHO_REPLY) {
        result = 0;
    } else if (reply->icmp6_type == ICMP6_TIME_EXCEEDED) {
        result = 1;
    } else {
        result = -1;
    }

    close(sock);
    return result;
}

int ri_icmp6_discover_neighbors(const char *iface, ri_graph_t *g)
{
    /* Send ICMPv6 multicast to all-nodes (ff02::1) on the interface */
    int sock = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
    if (sock < 0) {
        LOG_DEBUG("ICMPv6 socket for NDP: %s", strerror(errno));
        return -1;
    }

    unsigned int ifidx = if_nametoindex(iface);
    if (ifidx == 0) {
        close(sock);
        return -1;
    }

    setsockopt(sock, IPPROTO_IPV6, IPV6_MULTICAST_IF,
               &ifidx, sizeof(ifidx));

    struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    /* Send echo request to ff02::1 */
    struct sockaddr_in6 dst;
    memset(&dst, 0, sizeof(dst));
    dst.sin6_family = AF_INET6;
    inet_pton(AF_INET6, "ff02::1", &dst.sin6_addr);
    dst.sin6_scope_id = ifidx;

    struct icmp6_hdr pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.icmp6_type = ICMP6_ECHO_REQUEST;
    pkt.icmp6_id = htons(getpid() & 0xFFFF);
    pkt.icmp6_seq = htons(++g_seq6);

    sendto(sock, &pkt, sizeof(pkt), 0,
           (struct sockaddr *)&dst, sizeof(dst));

    /* Collect replies */
    int count = 0;
    for (int i = 0; i < 64; i++) {
        unsigned char buf[512];
        struct sockaddr_in6 from;
        socklen_t fromlen = sizeof(from);
        ssize_t n = recvfrom(sock, buf, sizeof(buf), 0,
                             (struct sockaddr *)&from, &fromlen);
        if (n < 0) break;

        struct icmp6_hdr *reply = (struct icmp6_hdr *)buf;
        if (reply->icmp6_type != ICMP6_ECHO_REPLY) continue;

        /* Check if already in graph */
        if (ri_graph_find_by_ipv6(g, &from.sin6_addr) >= 0) continue;

        /* Try to match to existing host by finding on same interface */
        ri_host_t h;
        ri_host_init(&h);
        memcpy(&h.ipv6, &from.sin6_addr, sizeof(struct in6_addr));
        h.has_ipv6 = 1;
        h.type = RI_HOST_LAN;
        ri_strlcpy(h.iface_name, iface, sizeof(h.iface_name));
        ri_graph_add_host(g, &h);
        count++;
    }

    close(sock);
    return count;
}
