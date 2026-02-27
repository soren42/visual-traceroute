#ifndef RI_DNS_H
#define RI_DNS_H

#include "core/graph.h"

/* Perform reverse DNS lookups for all hosts in the graph.
   Fills in dns_name and updates display_name. Returns count resolved. */
int ri_dns_resolve_all(ri_graph_t *g);

/* Reverse DNS for a single IPv4 address. Returns 0 on success. */
int ri_dns_reverse_ipv4(struct in_addr addr, char *buf, size_t buflen);

#endif /* RI_DNS_H */
