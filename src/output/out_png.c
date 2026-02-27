#include "config.h"
#ifdef HAVE_CAIRO

#include "output/out_png.h"
#include "output/layout.h"
#include "util/strutil.h"
#include "log.h"

#include <cairo.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define IMG_W 1920
#define IMG_H 1080
#define NODE_R 8.0
#define FONT_SIZE 10.0

static void color_for_type(ri_host_type_t t, double *r, double *g, double *b)
{
    switch (t) {
    case RI_HOST_LOCAL:   *r=0.2; *g=0.8; *b=0.2; break;
    case RI_HOST_GATEWAY: *r=1.0; *g=0.8; *b=0.0; break;
    case RI_HOST_LAN:     *r=0.3; *g=0.7; *b=1.0; break;
    case RI_HOST_REMOTE:  *r=0.8; *g=0.3; *b=0.8; break;
    case RI_HOST_TARGET:  *r=1.0; *g=0.2; *b=0.2; break;
    }
}

int ri_out_png(const ri_graph_t *g, const char *filename)
{
    cairo_surface_t *surface = cairo_image_surface_create(
        CAIRO_FORMAT_ARGB32, IMG_W, IMG_H);
    cairo_t *cr = cairo_create(surface);

    /* Dark background */
    cairo_set_source_rgb(cr, 0.1, 0.1, 0.15);
    cairo_paint(cr);

    /* Find bounds for auto-scaling */
    double minx = 1e9, maxx = -1e9, miny = 1e9, maxy = -1e9;
    for (int i = 0; i < g->host_count; i++) {
        if (g->hosts[i].x < minx) minx = g->hosts[i].x;
        if (g->hosts[i].x > maxx) maxx = g->hosts[i].x;
        if (g->hosts[i].y < miny) miny = g->hosts[i].y;
        if (g->hosts[i].y > maxy) maxy = g->hosts[i].y;
    }
    double dx = maxx - minx;
    double dy = maxy - miny;
    if (dx < 1) dx = 1;
    if (dy < 1) dy = 1;
    double scale = fmin((IMG_W - 200.0) / dx, (IMG_H - 200.0) / dy);
    double ox = IMG_W / 2.0 - (minx + dx / 2.0) * scale;
    double oy = IMG_H / 2.0 - (miny + dy / 2.0) * scale;

    /* Draw edges */
    for (int i = 0; i < g->edge_count; i++) {
        ri_edge_t *e = &g->edges[i];
        ri_host_t *s = &g->hosts[e->src_id];
        ri_host_t *d = &g->hosts[e->dst_id];

        double sx = s->x * scale + ox;
        double sy = s->y * scale + oy;
        double ex = d->x * scale + ox;
        double ey = d->y * scale + oy;

        if (e->in_mst) {
            cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.6);
            cairo_set_line_width(cr, 2.0);
        } else {
            cairo_set_source_rgba(cr, 0.5, 0.5, 0.5, 0.3);
            cairo_set_line_width(cr, 1.0);
        }
        cairo_move_to(cr, sx, sy);
        cairo_line_to(cr, ex, ey);
        cairo_stroke(cr);
    }

    /* Draw nodes */
    cairo_select_font_face(cr, "sans-serif",
                           CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, FONT_SIZE);

    for (int i = 0; i < g->host_count; i++) {
        ri_host_t *h = &g->hosts[i];
        double px = h->x * scale + ox;
        double py = h->y * scale + oy;

        double r, gr, b;
        color_for_type(h->type, &r, &gr, &b);

        /* Node circle */
        cairo_set_source_rgb(cr, r, gr, b);
        cairo_arc(cr, px, py, NODE_R, 0, 2 * M_PI);
        cairo_fill(cr);

        /* Border */
        cairo_set_source_rgb(cr, 1, 1, 1);
        cairo_set_line_width(cr, 1.0);
        cairo_arc(cr, px, py, NODE_R, 0, 2 * M_PI);
        cairo_stroke(cr);

        /* Label */
        cairo_set_source_rgb(cr, 1, 1, 1);
        cairo_move_to(cr, px + NODE_R + 4, py + 4);
        cairo_show_text(cr, h->display_name);
    }

    /* Title */
    cairo_set_font_size(cr, 16);
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_move_to(cr, 20, 30);
    char title[128];
    snprintf(title, sizeof(title), "RouteInspection - %d hosts, %d edges",
             g->host_count, g->edge_count);
    cairo_show_text(cr, title);

    /* Legend */
    cairo_set_font_size(cr, 11);
    const char *labels[] = {"Local", "Gateway", "LAN", "Remote", "Target"};
    ri_host_type_t types[] = {RI_HOST_LOCAL, RI_HOST_GATEWAY, RI_HOST_LAN,
                              RI_HOST_REMOTE, RI_HOST_TARGET};
    for (int i = 0; i < 5; i++) {
        double lr, lg, lb;
        color_for_type(types[i], &lr, &lg, &lb);
        cairo_set_source_rgb(cr, lr, lg, lb);
        cairo_arc(cr, 30, IMG_H - 100 + i * 20, 5, 0, 2 * M_PI);
        cairo_fill(cr);
        cairo_set_source_rgb(cr, 1, 1, 1);
        cairo_move_to(cr, 42, IMG_H - 96 + i * 20);
        cairo_show_text(cr, labels[i]);
    }

    cairo_surface_write_to_png(surface, filename);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);

    LOG_INFO("Wrote PNG to %s", filename);
    return 0;
}

#endif /* HAVE_CAIRO */
