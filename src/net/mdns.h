#ifndef RI_MDNS_H
#define RI_MDNS_H

#include "core/graph.h"

/* Browse for mDNS services and update hosts in graph.
   Returns number of mDNS names resolved. */
int ri_mdns_browse(ri_graph_t *g, int timeout_ms);

#endif /* RI_MDNS_H */
