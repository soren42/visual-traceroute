#ifndef RI_PING_H
#define RI_PING_H

#include "core/graph.h"
#include <netinet/in.h>

/* Ping sweep a subnet. Adds responding hosts to graph.
   base_addr is the network address, prefix_len is CIDR prefix.
   Returns number of hosts found. */
int ri_ping_sweep(ri_graph_t *g, struct in_addr base_addr,
                  int prefix_len, int timeout_ms);

#endif /* RI_PING_H */
