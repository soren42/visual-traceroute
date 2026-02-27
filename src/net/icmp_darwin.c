#include "config.h"
#ifdef RI_DARWIN

#include "net/icmp.h"
#include "log.h"

#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

static unsigned short g_seq = 0;

unsigned short ri_icmp_checksum(const void *data, int len)
{
    const unsigned short *p = data;
    unsigned int sum = 0;
    for (int i = 0; i < len / 2; i++)
        sum += p[i];
    if (len & 1)
        sum += ((const unsigned char *)data)[len - 1];
    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);
    return (unsigned short)(~sum);
}

static double now_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

double ri_icmp_ping(struct in_addr target, int timeout_ms)
{
    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sock < 0) {
        LOG_ERROR("ICMP socket: %s (need root?)", strerror(errno));
        return -1.0;
    }

    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct sockaddr_in dst;
    memset(&dst, 0, sizeof(dst));
    dst.sin_family = AF_INET;
    dst.sin_addr = target;

    struct icmp pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.icmp_type = ICMP_ECHO;
    pkt.icmp_code = 0;
    pkt.icmp_id = htons(getpid() & 0xFFFF);
    pkt.icmp_seq = htons(++g_seq);
    pkt.icmp_cksum = ri_icmp_checksum(&pkt, sizeof(pkt));

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

    if (n < 0) return -1.0;

    /* macOS: recv includes IP header */
    struct ip *iph = (struct ip *)buf;
    int ihl = iph->ip_hl << 2;
    if (n < ihl + (ssize_t)sizeof(struct icmp)) return -1.0;

    struct icmp *reply = (struct icmp *)(buf + ihl);
    if (reply->icmp_type != ICMP_ECHOREPLY) return -1.0;

    return t1 - t0;
}

int ri_icmp_probe(struct in_addr target, int ttl, int timeout_ms,
                  struct in_addr *reply_addr, double *rtt_ms)
{
    if (!reply_addr || !rtt_ms) return -1;

    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sock < 0) {
        LOG_ERROR("ICMP socket: %s", strerror(errno));
        return -1;
    }

    setsockopt(sock, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl));

    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct sockaddr_in dst;
    memset(&dst, 0, sizeof(dst));
    dst.sin_family = AF_INET;
    dst.sin_addr = target;

    struct icmp pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.icmp_type = ICMP_ECHO;
    pkt.icmp_code = 0;
    pkt.icmp_id = htons(getpid() & 0xFFFF);
    unsigned short seq = ++g_seq;
    pkt.icmp_seq = htons(seq);
    pkt.icmp_cksum = ri_icmp_checksum(&pkt, sizeof(pkt));

    double t0 = now_ms();
    if (sendto(sock, &pkt, sizeof(pkt), 0,
               (struct sockaddr *)&dst, sizeof(dst)) < 0) {
        close(sock);
        return -1;
    }

    unsigned char buf[512];
    ssize_t n = recv(sock, buf, sizeof(buf), 0);
    double t1 = now_ms();

    if (n < 0) {
        close(sock);
        return -1;
    }

    *rtt_ms = t1 - t0;

    /* macOS: IP header included in recv */
    struct ip *iph = (struct ip *)buf;
    int ihl = iph->ip_hl << 2;
    if (n < ihl + (ssize_t)sizeof(struct icmp)) {
        close(sock);
        return -1;
    }

    *reply_addr = iph->ip_src;
    struct icmp *reply = (struct icmp *)(buf + ihl);

    int result;
    if (reply->icmp_type == ICMP_ECHOREPLY &&
        reply->icmp_id == htons(getpid() & 0xFFFF)) {
        result = 0; /* reached target */
    } else if (reply->icmp_type == ICMP_TIMXCEED) {
        result = 1; /* intermediate hop */
    } else {
        result = -1;
    }

    close(sock);
    return result;
}

#endif /* RI_DARWIN */
