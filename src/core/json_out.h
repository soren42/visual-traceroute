#ifndef RI_JSON_OUT_H
#define RI_JSON_OUT_H

#include "core/graph.h"
#include "cJSON.h"

/* Serialize graph to cJSON object. Caller must cJSON_Delete. */
cJSON *ri_json_serialize(const ri_graph_t *g);

#endif /* RI_JSON_OUT_H */
