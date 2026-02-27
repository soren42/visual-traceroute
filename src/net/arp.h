#ifndef RI_ARP_H
#define RI_ARP_H

#include "core/graph.h"

/* Read ARP cache and add/update hosts in the graph.
   Returns number of ARP entries found. */
int ri_arp_read(ri_graph_t *g);

#endif /* RI_ARP_H */
