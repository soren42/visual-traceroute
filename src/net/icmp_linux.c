#include "config.h"
#ifdef RI_LINUX

#include "net/icmp.h"
#include "log.h"

#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
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
        LOG_ERROR("ICMP socket: %s", strerror(errno));
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

    struct icmphdr pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.type = ICMP_ECHO;
    pkt.code = 0;
    pkt.un.echo.id = htons(getpid() & 0xFFFF);
    pkt.un.echo.sequence = htons(++g_seq);
    pkt.checksum = ri_icmp_checksum(&pkt, sizeof(pkt));

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

    if (n < (ssize_t)sizeof(struct icmphdr)) return -1.0;

    /* Linux: no IP header in SOCK_RAW ICMP recv */
    struct icmphdr *reply = (struct icmphdr *)buf;
    if (reply->type != ICMP_ECHOREPLY) return -1.0;

    return t1 - t0;
}

int ri_icmp_probe(struct in_addr target, int ttl, int timeout_ms,
                  struct in_addr *reply_addr, double *rtt_ms)
{
    if (!reply_addr || !rtt_ms) return -1;

    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sock < 0) return -1;

    setsockopt(sock, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl));

    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct sockaddr_in dst;
    memset(&dst, 0, sizeof(dst));
    dst.sin_family = AF_INET;
    dst.sin_addr = target;

    struct icmphdr pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.type = ICMP_ECHO;
    pkt.code = 0;
    pkt.un.echo.id = htons(getpid() & 0xFFFF);
    unsigned short seq = ++g_seq;
    pkt.un.echo.sequence = htons(seq);
    pkt.checksum = ri_icmp_checksum(&pkt, sizeof(pkt));

    double t0 = now_ms();
    if (sendto(sock, &pkt, sizeof(pkt), 0,
               (struct sockaddr *)&dst, sizeof(dst)) < 0) {
        close(sock);
        return -1;
    }

    unsigned char buf[512];
    struct sockaddr_in from;
    socklen_t fromlen = sizeof(from);
    ssize_t n = recvfrom(sock, buf, sizeof(buf), 0,
                         (struct sockaddr *)&from, &fromlen);
    double t1 = now_ms();

    if (n < 0) { close(sock); return -1; }
    *rtt_ms = t1 - t0;
    *reply_addr = from.sin_addr;

    /* Linux: no IP header in recv buffer */
    struct icmphdr *reply = (struct icmphdr *)buf;
    int result;
    if (reply->type == ICMP_ECHOREPLY) {
        result = 0;
    } else if (reply->type == ICMP_TIME_EXCEEDED) {
        result = 1;
    } else {
        result = -1;
    }

    close(sock);
    return result;
}

#endif /* RI_LINUX */
