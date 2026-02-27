#ifndef RI_OUT_HTML_H
#define RI_OUT_HTML_H

#include "core/graph.h"

/* Generate self-contained HTML file with Three.js 3D visualization.
   Returns 0 on success. */
int ri_out_html(const ri_graph_t *g, const char *filename);

#endif /* RI_OUT_HTML_H */
