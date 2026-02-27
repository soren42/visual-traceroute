#ifndef RI_OUT_PNG_H
#define RI_OUT_PNG_H

#include "core/graph.h"

/* Render graph as PNG image. Returns 0 on success. */
int ri_out_png(const ri_graph_t *g, const char *filename);

#endif /* RI_OUT_PNG_H */
