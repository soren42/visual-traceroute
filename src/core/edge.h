#ifndef RI_EDGE_H
#define RI_EDGE_H

typedef enum {
    RI_EDGE_LAN     = 0,
    RI_EDGE_ROUTE   = 1,
    RI_EDGE_GATEWAY = 2
} ri_edge_type_t;

typedef struct {
    int             src_id;
    int             dst_id;
    double          weight;   /* RTT in ms, or 1.0 for LAN */
    ri_edge_type_t  type;
    int             in_mst;
} ri_edge_t;

/* Initialize an edge */
void ri_edge_init(ri_edge_t *e, int src, int dst, double weight,
                  ri_edge_type_t type);

/* Get type as string */
const char *ri_edge_type_str(ri_edge_type_t type);

#endif /* RI_EDGE_H */
