#ifndef RI_DNS_H
#define RI_DNS_H

#include "core/graph.h"

/* Perform reverse DNS lookups for all hosts in the graph.
   If nameserver is non-NULL and non-empty, queries that server via UDP.
   Otherwise uses system resolver (getnameinfo).
   Fills in dns_name and updates display_name. Returns count resolved. */
int ri_dns_resolve_all(ri_graph_t *g, const char *nameserver);

/* Reverse DNS for a single IPv4 address using system resolver.
   Returns 0 on success. */
int ri_dns_reverse_ipv4(struct in_addr addr, char *buf, size_t buflen);

/* Reverse DNS for a single IPv4 address using a specific nameserver.
   Returns 0 on success. */
int ri_dns_reverse_ipv4_ns(struct in_addr ns_addr, struct in_addr target,
                           char *buf, size_t buflen);

#endif /* RI_DNS_H */
