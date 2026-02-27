#ifndef RI_OUT_MP4_H
#define RI_OUT_MP4_H

#include "core/graph.h"

/* Render graph as rotating 3D MP4 video. Returns 0 on success. */
int ri_out_mp4(const ri_graph_t *g, const char *filename);

#endif /* RI_OUT_MP4_H */
