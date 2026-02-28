#ifndef RI_OUT_HTML_H
#define RI_OUT_HTML_H

#include "core/graph.h"

/* Generate self-contained HTML file with Three.js 3D visualization.
   Returns 0 on success. */
int ri_out_html(const ri_graph_t *g, const char *filename);

/* Same as ri_out_html but returns a malloc'd HTML string (caller frees).
   Returns NULL on error. */
char *ri_out_html_string(const ri_graph_t *g);

#endif /* RI_OUT_HTML_H */
