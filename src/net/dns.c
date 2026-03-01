#include "net/dns.h"
#include "log.h"
#include "util/strutil.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <poll.h>
#include <stdint.h>

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

/* Encode a dotted domain name into DNS wire label format.
   Returns bytes written, or -1 on error. */
static int encode_dns_name(const char *name, uint8_t *buf, size_t buflen)
{
    const char *p = name;
    uint8_t *out = buf;
    size_t total = 0;

    while (*p) {
        const char *dot = strchr(p, '.');
        size_t len = dot ? (size_t)(dot - p) : strlen(p);
        if (len > 63 || total + len + 2 > buflen) return -1;
        *out++ = (uint8_t)len;
        memcpy(out, p, len);
        out += len;
        total += len + 1;
        p += len;
        if (*p == '.') p++;
    }
    *out++ = 0; /* root label */
    total++;
    return (int)total;
}

/* Decode a DNS name from a response, handling compression pointers.
   Returns bytes consumed from ptr, or -1 on error. */
static int decode_dns_name(const uint8_t *msg, size_t msglen,
                           const uint8_t *ptr, char *buf, size_t buflen)
{
    size_t pos = 0;
    int jumped = 0;
    int consumed = 0;
    const uint8_t *p = ptr;
    int max_jumps = 50;

    while (p < msg + msglen && *p && max_jumps-- > 0) {
        if ((*p & 0xC0) == 0xC0) {
            if (p + 1 >= msg + msglen) return -1;
            uint16_t offset = (uint16_t)((*p & 0x3F) << 8) | *(p + 1);
            if (offset >= msglen) return -1;
            if (!jumped) consumed = (int)(p - ptr) + 2;
            p = msg + offset;
            jumped = 1;
            continue;
        }

        uint8_t len = *p++;
        if (p + len > msg + msglen) return -1;
        if (pos + len + 1 >= buflen) return -1;

        if (pos > 0) buf[pos++] = '.';
        memcpy(buf + pos, p, len);
        pos += len;
        p += len;
    }

    buf[pos] = '\0';
    if (!jumped) consumed = (int)(p - ptr) + 1;
    return consumed;
}

int ri_dns_reverse_ipv4_ns(struct in_addr ns_addr, struct in_addr target,
                           char *buf, size_t buflen)
{
    /* Build PTR domain: reverse the octets and append .in-addr.arpa */
    uint32_t ip = ntohl(target.s_addr);
    char ptr_name[256];
    snprintf(ptr_name, sizeof(ptr_name), "%u.%u.%u.%u.in-addr.arpa",
             ip & 0xFF, (ip >> 8) & 0xFF,
             (ip >> 16) & 0xFF, (ip >> 24) & 0xFF);

    /* Build DNS query packet */
    uint8_t query[512];
    memset(query, 0, sizeof(query));

    static uint16_t dns_id = 0;
    uint16_t id = ++dns_id;
    query[0] = (uint8_t)(id >> 8);
    query[1] = (uint8_t)(id & 0xFF);
    query[2] = 0x01; /* RD (Recursion Desired) */
    query[3] = 0x00;
    query[4] = 0x00; query[5] = 0x01; /* QDCOUNT = 1 */

    int namelen = encode_dns_name(ptr_name, query + 12, sizeof(query) - 16);
    if (namelen < 0) return -1;

    int qend = 12 + namelen;
    query[qend]     = 0x00; query[qend + 1] = 0x0C; /* QTYPE = PTR */
    query[qend + 2] = 0x00; query[qend + 3] = 0x01; /* QCLASS = IN */
    int querylen = qend + 4;

    /* Send UDP query */
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return -1;

    struct sockaddr_in ns;
    memset(&ns, 0, sizeof(ns));
    ns.sin_family = AF_INET;
    ns.sin_port = htons(53);
    ns.sin_addr = ns_addr;

    if (sendto(sock, query, (size_t)querylen, 0,
               (struct sockaddr *)&ns, sizeof(ns)) < 0) {
        close(sock);
        return -1;
    }

    /* Wait for response */
    struct pollfd pfd = { .fd = sock, .events = POLLIN };
    if (poll(&pfd, 1, 2000) <= 0) {
        close(sock);
        return -1;
    }

    uint8_t resp[1024];
    ssize_t rlen = recv(sock, resp, sizeof(resp), 0);
    close(sock);

    if (rlen < 12) return -1;

    /* Validate response */
    if (resp[0] != query[0] || resp[1] != query[1]) return -1;
    if (!(resp[2] & 0x80)) return -1; /* QR must be 1 */
    if ((resp[3] & 0x0F) != 0) return -1; /* RCODE must be NOERROR */

    uint16_t ancount = (uint16_t)(resp[6] << 8) | resp[7];
    if (ancount == 0) return -1;

    /* Skip question section */
    const uint8_t *p = resp + 12;
    const uint8_t *end = resp + rlen;

    while (p < end && *p) {
        if ((*p & 0xC0) == 0xC0) { p += 2; goto past_qname; }
        p += 1 + *p;
    }
    if (p < end) p++; /* null label */
past_qname:
    p += 4; /* QTYPE + QCLASS */

    if (p >= end) return -1;

    /* Parse first answer */
    char dummy[256];
    int skip = decode_dns_name(resp, (size_t)rlen, p, dummy, sizeof(dummy));
    if (skip < 0) return -1;
    p += skip;

    if (p + 10 > end) return -1;

    uint16_t atype = (uint16_t)(p[0] << 8) | p[1];
    p += 10; /* TYPE(2) + CLASS(2) + TTL(4) + RDLENGTH(2) */

    if (atype != 12) return -1; /* Not PTR */
    if (p >= end) return -1;

    /* Decode PTR RDATA */
    if (decode_dns_name(resp, (size_t)rlen, p, buf, buflen) < 0)
        return -1;

    /* Remove trailing dot */
    size_t slen = strlen(buf);
    if (slen > 0 && buf[slen - 1] == '.')
        buf[slen - 1] = '\0';

    return 0;
}

int ri_dns_resolve_all(ri_graph_t *g, const char *nameserver)
{
    struct in_addr ns_addr;
    int use_custom = 0;

    if (nameserver && nameserver[0] != '\0') {
        if (inet_pton(AF_INET, nameserver, &ns_addr) == 1) {
            use_custom = 1;
            LOG_INFO("Using custom DNS server: %s", nameserver);
        } else {
            LOG_WARN("Invalid nameserver IP '%s', using system resolver",
                     nameserver);
        }
    }

    int count = 0;
    for (int i = 0; i < g->host_count; i++) {
        ri_host_t *h = &g->hosts[i];
        if (!h->has_ipv4) continue;
        if (!ri_str_empty(h->dns_name)) continue;

        char name[RI_HOSTNAME_LEN];
        int rc;

        if (use_custom)
            rc = ri_dns_reverse_ipv4_ns(ns_addr, h->ipv4, name, sizeof(name));
        else
            rc = ri_dns_reverse_ipv4(h->ipv4, name, sizeof(name));

        if (rc == 0) {
            ri_strlcpy(h->dns_name, name, sizeof(h->dns_name));
            LOG_DEBUG("DNS: %s -> %s", ri_host_ipv4_str(h), name);
            count++;
        }
    }
    return count;
}
