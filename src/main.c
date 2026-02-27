#include "config.h"
#include "cli.h"
#include "log.h"
#include "core/scan.h"
#include "core/graph.h"
#include "output/out_json.h"
#include "output/out_html.h"
#include "output/layout.h"

#ifdef HAVE_NCURSES
#include "output/out_curses.h"
#endif
#ifdef HAVE_CAIRO
#include "output/out_png.h"
#endif
#if defined(HAVE_FFMPEG) && defined(HAVE_CAIRO)
#include "output/out_mp4.h"
#endif

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    ri_config_t cfg;
    if (ri_cli_parse(&cfg, argc, argv) != 0) {
        ri_cli_usage(argv[0]);
        return 1;
    }

    ri_log_set_level(cfg.verbosity);
    LOG_INFO("visual-traceroute %s starting", RI_VERSION);
    LOG_DEBUG("Verbosity: %d, Output flags: 0x%x", cfg.verbosity,
              cfg.output_flags);

    /* Run discovery */
    ri_graph_t *g = ri_scan_run(&cfg);
    if (!g) {
        LOG_ERROR("Discovery failed");
        return 1;
    }

    LOG_INFO("Discovered %d hosts, %d edges", g->host_count, g->edge_count);

    /* Compute MST */
    int mst_edges = ri_graph_kruskal_mst(g);
    LOG_INFO("MST: %d edges", mst_edges);

    /* Compute layout if needed for visual outputs */
    if (cfg.output_flags & (RI_OUT_PNG | RI_OUT_MP4 | RI_OUT_HTML)) {
        ri_layout_3d(g);
        ri_layout_force_refine(g, 50);
        LOG_INFO("Layout computed");
    }

    /* Generate outputs */
    char path[512];

    if (cfg.output_flags & RI_OUT_JSON) {
        snprintf(path, sizeof(path), "%s.json", cfg.file_base);
        ri_out_json(g, path);
    }

#ifdef HAVE_NCURSES
    if (cfg.output_flags & RI_OUT_CURSES) {
        ri_out_curses(g);
    }
#else
    if (cfg.output_flags & RI_OUT_CURSES) {
        LOG_WARN("Curses output not available (ncurses not found)");
    }
#endif

#ifdef HAVE_CAIRO
    if (cfg.output_flags & RI_OUT_PNG) {
        snprintf(path, sizeof(path), "%s.png", cfg.file_base);
        if (!(cfg.output_flags & (RI_OUT_MP4 | RI_OUT_HTML))) {
            ri_layout_radial_2d(g);
            ri_layout_force_refine(g, 50);
        }
        ri_out_png(g, path);
    }
#else
    if (cfg.output_flags & RI_OUT_PNG) {
        LOG_WARN("PNG output not available (cairo not found)");
    }
#endif

#if defined(HAVE_FFMPEG) && defined(HAVE_CAIRO)
    if (cfg.output_flags & RI_OUT_MP4) {
        snprintf(path, sizeof(path), "%s.mp4", cfg.file_base);
        ri_out_mp4(g, path);
    }
#else
    if (cfg.output_flags & RI_OUT_MP4) {
        LOG_WARN("MP4 output not available (ffmpeg/cairo not found)");
    }
#endif

    if (cfg.output_flags & RI_OUT_HTML) {
        snprintf(path, sizeof(path), "%s.html", cfg.file_base);
        ri_out_html(g, path);
    }

    ri_graph_destroy(g);
    LOG_INFO("Done");
    return 0;
}
