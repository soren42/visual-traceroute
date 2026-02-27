#ifndef RI_LAYOUT_H
#define RI_LAYOUT_H

#include "core/graph.h"

/* Compute 2D radial tree layout. Updates x,y coordinates on hosts.
   Returns 0 on success. */
int ri_layout_radial_2d(ri_graph_t *g);

/* Compute 3D layout with traceroute spine along Z axis.
   Returns 0 on success. */
int ri_layout_3d(ri_graph_t *g);

/* Apply Fruchterman-Reingold force-directed refinement.
   iterations: number of FR passes (50 recommended). */
void ri_layout_force_refine(ri_graph_t *g, int iterations);

#endif /* RI_LAYOUT_H */
