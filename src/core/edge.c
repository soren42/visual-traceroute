#include "core/edge.h"

void ri_edge_init(ri_edge_t *e, int src, int dst, double weight,
                  ri_edge_type_t type)
{
    e->src_id = src;
    e->dst_id = dst;
    e->weight = weight;
    e->type   = type;
    e->in_mst = 0;
}

const char *ri_edge_type_str(ri_edge_type_t type)
{
    switch (type) {
        case RI_EDGE_LAN:     return "lan";
        case RI_EDGE_ROUTE:   return "route";
        case RI_EDGE_GATEWAY: return "gateway";
    }
    return "unknown";
}
