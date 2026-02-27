#ifndef RI_ICMP6_H
#define RI_ICMP6_H

#include "core/graph.h"
#include <netinet/in.h>

/* Send ICMPv6 echo and wait for reply. Returns RTT in ms, or -1. */
double ri_icmp6_ping(const struct in6_addr *target, int timeout_ms);

/* Send ICMPv6 echo with specified hop limit. Returns same as ri_icmp_probe. */
int ri_icmp6_probe(const struct in6_addr *target, int hop_limit,
                   int timeout_ms, struct in6_addr *reply_addr,
                   double *rtt_ms);

/* Perform IPv6 neighbor discovery on a given interface.
   Adds discovered hosts to graph. Returns count. */
int ri_icmp6_discover_neighbors(const char *iface, ri_graph_t *g);

#endif /* RI_ICMP6_H */
