#ifndef RI_OUT_JSON_H
#define RI_OUT_JSON_H

#include "core/graph.h"

/* Write graph as JSON to file. Returns 0 on success. */
int ri_out_json(const ri_graph_t *g, const char *filename);

#endif /* RI_OUT_JSON_H */
