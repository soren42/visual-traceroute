#ifndef RI_GRAPH_H
#define RI_GRAPH_H

#include "core/host.h"
#include "core/edge.h"

/* Adjacency list node */
typedef struct ri_adj_node {
    int                  edge_idx;
    struct ri_adj_node  *next;
} ri_adj_node_t;

typedef struct {
    ri_host_t       *hosts;
    int              host_count;
    int              host_cap;

    ri_edge_t       *edges;
    int              edge_count;
    int              edge_cap;

    ri_adj_node_t  **adj;    /* adj[host_id] → linked list of edge indices */
    int              adj_cap;
} ri_graph_t;

/* Create/destroy graph */
ri_graph_t *ri_graph_create(void);
void        ri_graph_destroy(ri_graph_t *g);

/* Add a host, returns host ID */
int ri_graph_add_host(ri_graph_t *g, const ri_host_t *h);

/* Find host by IPv4 address, returns host ID or -1 */
int ri_graph_find_by_ipv4(const ri_graph_t *g, struct in_addr addr);

/* Find host by IPv6 address, returns host ID or -1 */
int ri_graph_find_by_ipv6(const ri_graph_t *g, const struct in6_addr *addr);

/* Find host by MAC address, returns host ID or -1 */
int ri_graph_find_by_mac(const ri_graph_t *g, const unsigned char *mac);

/* Find host by interface name, returns host ID or -1 */
int ri_graph_find_by_iface(const ri_graph_t *g, const char *iface);

/* Add an edge, returns edge index */
int ri_graph_add_edge(ri_graph_t *g, int src_id, int dst_id,
                      double weight, ri_edge_type_t type);

/* Check if edge exists between two hosts */
int ri_graph_has_edge(const ri_graph_t *g, int src_id, int dst_id);

/* Compute Kruskal MST, marks in_mst on edges. Returns MST edge count. */
int ri_graph_kruskal_mst(ri_graph_t *g);

/* BFS from a source host over MST edges. Fills parent[] and depth[].
   Arrays must be pre-allocated to host_count size. Returns 0 on success. */
int ri_graph_bfs_mst(const ri_graph_t *g, int src_id,
                     int *parent, int *depth);

#endif /* RI_GRAPH_H */
