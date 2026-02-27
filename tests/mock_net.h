#ifndef RI_MOCK_NET_H
#define RI_MOCK_NET_H

#include "core/graph.h"

/* Build a sample graph for testing.
   Creates: local host, gateway, 3 LAN hosts, 2 remote hops, 1 target.
   Returns a graph with 8 hosts and appropriate edges. */
ri_graph_t *mock_build_sample_graph(void);

#endif /* RI_MOCK_NET_H */
