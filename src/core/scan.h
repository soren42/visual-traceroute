#ifndef RI_SCAN_H
#define RI_SCAN_H

#include "core/graph.h"
#include "cli.h"

/* Progress callback — called with a human-readable status message. */
typedef void (*ri_progress_fn)(const char *msg, void *ctx);

/* Set (or clear) the progress callback. Thread-unsafe by design. */
void ri_scan_set_progress(ri_progress_fn fn, void *ctx);

/* Run the full discovery engine. Returns populated graph. */
ri_graph_t *ri_scan_run(const ri_config_t *cfg);

/* Individual scan phases (for fine-grained control) */
int ri_scan_local_interfaces(ri_graph_t *g, const ri_config_t *cfg);
int ri_scan_routing_table(ri_graph_t *g, const ri_config_t *cfg);
int ri_scan_lan_discovery(ri_graph_t *g, const ri_config_t *cfg);
int ri_scan_name_resolution(ri_graph_t *g, const ri_config_t *cfg);
int ri_scan_traceroute(ri_graph_t *g, const ri_config_t *cfg);
int ri_scan_ipv6_augment(ri_graph_t *g, const ri_config_t *cfg);

#endif /* RI_SCAN_H */
