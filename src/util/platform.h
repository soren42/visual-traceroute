#ifndef RI_PLATFORM_H
#define RI_PLATFORM_H

#include "config.h"

#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifdef RI_DARWIN
  #include <netinet/ip.h>
  #include <netinet/ip_icmp.h>
  /* macOS uses struct icmp from <netinet/ip_icmp.h> */
  #define RI_ICMP_TYPE(p)     ((p)->icmp_type)
  #define RI_ICMP_CODE(p)     ((p)->icmp_code)
  #define RI_ICMP_ID(p)       ((p)->icmp_hun.ih_idseq.icd_id)
  #define RI_ICMP_SEQ(p)      ((p)->icmp_hun.ih_idseq.icd_seq)
  #define RI_ICMP_CKSUM(p)    ((p)->icmp_cksum)
  typedef struct icmp ri_icmp_t;
  #define RI_ICMP_ECHO_REQUEST  ICMP_ECHO
  #define RI_ICMP_ECHO_REPLY    ICMP_ECHOREPLY
  #define RI_ICMP_TIME_EXCEEDED ICMP_TIMXCEED
  /* macOS raw socket includes IP header in recv buffer */
  #define RI_ICMP_RECV_HAS_IP_HDR 1
#endif

#ifdef RI_LINUX
  #include <netinet/ip_icmp.h>
  /* Linux uses struct icmphdr */
  #define RI_ICMP_TYPE(p)     ((p)->type)
  #define RI_ICMP_CODE(p)     ((p)->code)
  #define RI_ICMP_ID(p)       ((p)->un.echo.id)
  #define RI_ICMP_SEQ(p)      ((p)->un.echo.sequence)
  #define RI_ICMP_CKSUM(p)    ((p)->checksum)
  typedef struct icmphdr ri_icmp_t;
  #define RI_ICMP_ECHO_REQUEST  ICMP_ECHO
  #define RI_ICMP_ECHO_REPLY    ICMP_ECHOREPLY
  #define RI_ICMP_TIME_EXCEEDED ICMP_TIME_EXCEEDED
  /* Linux raw socket does NOT include IP header in recv */
  #define RI_ICMP_RECV_HAS_IP_HDR 0
#endif

/* Common constants */
#define RI_MAC_LEN       6
#define RI_MAC_STR_LEN   18  /* "aa:bb:cc:dd:ee:ff\0" */
#define RI_IPV4_STR_LEN  INET_ADDRSTRLEN
#define RI_IPV6_STR_LEN  INET6_ADDRSTRLEN
#define RI_HOSTNAME_LEN  256
#define RI_MAX_HOPS      30
#define RI_PING_TIMEOUT_MS 1000
#define RI_PING_PAYLOAD_SIZE 56

#endif /* RI_PLATFORM_H */
