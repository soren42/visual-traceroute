#ifndef RI_ROUTE_H
#define RI_ROUTE_H

#include "core/graph.h"

/* Read the system routing table. Adds gateway hosts and edges.
   Returns number of routes found. */
int ri_route_read(ri_graph_t *g);

#endif /* RI_ROUTE_H */
