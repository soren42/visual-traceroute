#include "output/layout.h"
#include "util/alloc.h"
#include "log.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

int ri_layout_radial_2d(ri_graph_t *g)
{
    if (g->host_count == 0) return 0;

    /* Compute MST if not done */
    ri_graph_kruskal_mst(g);

    /* BFS from first local host */
    int src = 0;
    for (int i = 0; i < g->host_count; i++) {
        if (g->hosts[i].type == RI_HOST_LOCAL) { src = i; break; }
    }

    int *parent = ri_calloc((size_t)g->host_count, sizeof(int));
    int *depth = ri_calloc((size_t)g->host_count, sizeof(int));
    ri_graph_bfs_mst(g, src, parent, depth);

    /* Count children per depth level */
    int max_depth = 0;
    for (int i = 0; i < g->host_count; i++) {
        if (depth[i] > max_depth) max_depth = depth[i];
    }

    /* Track how many nodes placed at each depth */
    int *count_at = ri_calloc((size_t)(max_depth + 1), sizeof(int));
    int *placed_at = ri_calloc((size_t)(max_depth + 1), sizeof(int));
    for (int i = 0; i < g->host_count; i++) {
        if (depth[i] >= 0) count_at[depth[i]]++;
    }

    /* Place root at origin */
    double radius_step = 150.0;

    for (int i = 0; i < g->host_count; i++) {
        int d = depth[i];
        if (d < 0) {
            /* Disconnected node - place far out */
            g->hosts[i].x = 500.0 + i * 30.0;
            g->hosts[i].y = 500.0;
            continue;
        }
        if (d == 0) {
            g->hosts[i].x = 0;
            g->hosts[i].y = 0;
        } else {
            double r = d * radius_step;
            int n = count_at[d];
            int k = placed_at[d];
            double angle;

            if (g->hosts[i].type == RI_HOST_GATEWAY) {
                /* Gateways in upper half [0, pi] */
                angle = (n > 1) ? (M_PI * k / (n - 1)) : (M_PI / 2);
            } else {
                /* LAN hosts in lower half [pi, 2*pi] */
                angle = M_PI + (n > 1 ? (M_PI * k / (n - 1)) : (M_PI / 2));
            }

            g->hosts[i].x = r * cos(angle);
            g->hosts[i].y = r * sin(angle);
            placed_at[d]++;
        }
    }

    ri_free(parent);
    ri_free(depth);
    ri_free(count_at);
    ri_free(placed_at);
    return 0;
}

int ri_layout_3d(ri_graph_t *g)
{
    if (g->host_count == 0) return 0;

    ri_graph_kruskal_mst(g);

    int src = 0;
    for (int i = 0; i < g->host_count; i++) {
        if (g->hosts[i].type == RI_HOST_LOCAL) { src = i; break; }
    }

    int *parent = ri_calloc((size_t)g->host_count, sizeof(int));
    int *depth = ri_calloc((size_t)g->host_count, sizeof(int));
    ri_graph_bfs_mst(g, src, parent, depth);

    int max_depth = 0;
    for (int i = 0; i < g->host_count; i++) {
        if (depth[i] > max_depth) max_depth = depth[i];
    }

    int *count_at = ri_calloc((size_t)(max_depth + 1), sizeof(int));
    int *placed_at = ri_calloc((size_t)(max_depth + 1), sizeof(int));
    for (int i = 0; i < g->host_count; i++) {
        if (depth[i] >= 0) count_at[depth[i]]++;
    }

    double z_step = 100.0;
    double radius = 80.0;

    for (int i = 0; i < g->host_count; i++) {
        int d = depth[i];
        if (d < 0) {
            g->hosts[i].x = 200.0;
            g->hosts[i].y = 200.0;
            g->hosts[i].z = 0;
            continue;
        }

        /* Z axis = depth (traceroute spine) */
        g->hosts[i].z = d * z_step;

        if (d == 0) {
            g->hosts[i].x = 0;
            g->hosts[i].y = 0;
        } else {
            /* Radial distribution in XY plane at each depth */
            int n = count_at[d];
            int k = placed_at[d]++;
            double angle = (n > 1) ? (2.0 * M_PI * k / n) : 0;
            double r = (g->hosts[i].type == RI_HOST_REMOTE) ?
                       radius * 0.3 : radius;

            g->hosts[i].x = r * cos(angle);
            g->hosts[i].y = r * sin(angle);
        }
    }

    ri_free(parent);
    ri_free(depth);
    ri_free(count_at);
    ri_free(placed_at);
    return 0;
}

void ri_layout_force_refine(ri_graph_t *g, int iterations)
{
    if (g->host_count < 2) return;

    double *dx = ri_calloc((size_t)g->host_count, sizeof(double));
    double *dy = ri_calloc((size_t)g->host_count, sizeof(double));
    double *dz = ri_calloc((size_t)g->host_count, sizeof(double));

    double area = 500.0 * 500.0;
    double k = sqrt(area / g->host_count);

    for (int iter = 0; iter < iterations; iter++) {
        double temp = k * (1.0 - (double)iter / iterations);

        memset(dx, 0, (size_t)g->host_count * sizeof(double));
        memset(dy, 0, (size_t)g->host_count * sizeof(double));
        memset(dz, 0, (size_t)g->host_count * sizeof(double));

        /* Repulsive forces */
        for (int i = 0; i < g->host_count; i++) {
            for (int j = i + 1; j < g->host_count; j++) {
                double ex = g->hosts[i].x - g->hosts[j].x;
                double ey = g->hosts[i].y - g->hosts[j].y;
                double ez = g->hosts[i].z - g->hosts[j].z;
                double dist = sqrt(ex*ex + ey*ey + ez*ez);
                if (dist < 0.01) dist = 0.01;

                double force = (k * k) / dist;
                double fx = (ex / dist) * force;
                double fy = (ey / dist) * force;
                double fz = (ez / dist) * force;

                dx[i] += fx; dy[i] += fy; dz[i] += fz;
                dx[j] -= fx; dy[j] -= fy; dz[j] -= fz;
            }
        }

        /* Attractive forces along edges */
        for (int e = 0; e < g->edge_count; e++) {
            int si = g->edges[e].src_id;
            int di = g->edges[e].dst_id;
            double ex = g->hosts[si].x - g->hosts[di].x;
            double ey = g->hosts[si].y - g->hosts[di].y;
            double ez = g->hosts[si].z - g->hosts[di].z;
            double dist = sqrt(ex*ex + ey*ey + ez*ez);
            if (dist < 0.01) dist = 0.01;

            double force = (dist * dist) / k;
            double fx = (ex / dist) * force;
            double fy = (ey / dist) * force;
            double fz = (ez / dist) * force;

            dx[si] -= fx; dy[si] -= fy; dz[si] -= fz;
            dx[di] += fx; dy[di] += fy; dz[di] += fz;
        }

        /* Apply displacements with temperature limiting */
        for (int i = 0; i < g->host_count; i++) {
            /* Don't move the root (local host) */
            if (g->hosts[i].type == RI_HOST_LOCAL) continue;

            double dist = sqrt(dx[i]*dx[i] + dy[i]*dy[i] + dz[i]*dz[i]);
            if (dist < 0.01) continue;

            double scale = (dist < temp) ? 1.0 : (temp / dist);
            g->hosts[i].x += dx[i] * scale * 0.1;
            g->hosts[i].y += dy[i] * scale * 0.1;
            g->hosts[i].z += dz[i] * scale * 0.1;
        }
    }

    ri_free(dx);
    ri_free(dy);
    ri_free(dz);
}
