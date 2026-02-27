#ifndef RI_IFACE_H
#define RI_IFACE_H

#include "core/graph.h"

/* Enumerate local network interfaces and add them to the graph.
   Returns number of interfaces found. */
int ri_iface_enumerate(ri_graph_t *g);

#endif /* RI_IFACE_H */
