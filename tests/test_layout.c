#include "output/layout.h"
#include "mock_net.h"
#include <stdio.h>
#include <math.h>

extern int g_tests_run, g_tests_passed, g_tests_failed;
#define TEST_ASSERT(cond, msg) do { \
    g_tests_run++; \
    if (!(cond)) { \
        fprintf(stderr, "  FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
        g_tests_failed++; \
    } else { \
        g_tests_passed++; \
    } \
} while(0)

static void test_layout_radial_2d(void)
{
    ri_graph_t *g = mock_build_sample_graph();
    int rc = ri_layout_radial_2d(g);
    TEST_ASSERT(rc == 0, "radial 2d returns 0");

    /* Root should be at origin */
    TEST_ASSERT(fabs(g->hosts[0].x) < 0.01, "root x near 0");
    TEST_ASSERT(fabs(g->hosts[0].y) < 0.01, "root y near 0");

    /* Non-root hosts should be displaced */
    int displaced = 0;
    for (int i = 1; i < g->host_count; i++) {
        double d = sqrt(g->hosts[i].x * g->hosts[i].x +
                        g->hosts[i].y * g->hosts[i].y);
        if (d > 1.0) displaced++;
    }
    TEST_ASSERT(displaced > 0, "some hosts displaced from origin");

    ri_graph_destroy(g);
}

static void test_layout_3d(void)
{
    ri_graph_t *g = mock_build_sample_graph();
    int rc = ri_layout_3d(g);
    TEST_ASSERT(rc == 0, "3d layout returns 0");

    /* Root at z=0 */
    TEST_ASSERT(fabs(g->hosts[0].z) < 0.01, "root z near 0");

    /* Remote hosts should have positive z (depth) */
    int has_depth = 0;
    for (int i = 0; i < g->host_count; i++) {
        if (g->hosts[i].type == RI_HOST_REMOTE && g->hosts[i].z > 0)
            has_depth++;
    }
    TEST_ASSERT(has_depth > 0, "remote hosts have positive z");

    ri_graph_destroy(g);
}

static void test_layout_force_refine(void)
{
    ri_graph_t *g = mock_build_sample_graph();
    ri_layout_3d(g);

    /* Record positions before */
    double x0 = g->hosts[2].x;
    double y0 = g->hosts[2].y;

    ri_layout_force_refine(g, 50);

    /* Positions should change (force-directed moves nodes) */
    double x1 = g->hosts[2].x;
    double y1 = g->hosts[2].y;
    double delta = fabs(x1 - x0) + fabs(y1 - y0);
    TEST_ASSERT(delta > 0.001, "force refinement moves nodes");

    ri_graph_destroy(g);
}

void test_layout_suite(void)
{
    test_layout_radial_2d();
    test_layout_3d();
    test_layout_force_refine();
}
