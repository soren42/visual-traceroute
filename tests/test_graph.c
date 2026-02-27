#include "core/graph.h"
#include "core/host.h"
#include "core/edge.h"
#include "mock_net.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* From test_main.c */
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
#define TEST_ASSERT_EQ(a, b, msg) TEST_ASSERT((a) == (b), msg)

static void test_graph_create_destroy(void)
{
    ri_graph_t *g = ri_graph_create();
    TEST_ASSERT(g != NULL, "graph creation");
    TEST_ASSERT_EQ(g->host_count, 0, "initial host count");
    TEST_ASSERT_EQ(g->edge_count, 0, "initial edge count");
    ri_graph_destroy(g);
}

static void test_graph_add_host(void)
{
    ri_graph_t *g = ri_graph_create();
    ri_host_t h;
    ri_host_init(&h);
    ri_host_set_ipv4(&h, "192.168.1.1");
    int id = ri_graph_add_host(g, &h);
    TEST_ASSERT_EQ(id, 0, "first host id is 0");
    TEST_ASSERT_EQ(g->host_count, 1, "host count after add");
    TEST_ASSERT_EQ(g->hosts[0].id, 0, "host id stored");
    TEST_ASSERT(g->hosts[0].has_ipv4, "host has ipv4");
    ri_graph_destroy(g);
}

static void test_graph_find_by_ipv4(void)
{
    ri_graph_t *g = ri_graph_create();
    ri_host_t h;
    ri_host_init(&h);
    ri_host_set_ipv4(&h, "10.0.0.1");
    ri_graph_add_host(g, &h);
    ri_host_init(&h);
    ri_host_set_ipv4(&h, "10.0.0.2");
    ri_graph_add_host(g, &h);

    struct in_addr addr;
    inet_pton(AF_INET, "10.0.0.2", &addr);
    int id = ri_graph_find_by_ipv4(g, addr);
    TEST_ASSERT_EQ(id, 1, "find second host");

    inet_pton(AF_INET, "10.0.0.99", &addr);
    id = ri_graph_find_by_ipv4(g, addr);
    TEST_ASSERT_EQ(id, -1, "not found returns -1");
    ri_graph_destroy(g);
}

static void test_graph_add_edge(void)
{
    ri_graph_t *g = ri_graph_create();
    ri_host_t h;
    ri_host_init(&h);
    ri_host_set_ipv4(&h, "1.1.1.1");
    ri_graph_add_host(g, &h);
    ri_host_init(&h);
    ri_host_set_ipv4(&h, "2.2.2.2");
    ri_graph_add_host(g, &h);

    int eid = ri_graph_add_edge(g, 0, 1, 5.0, RI_EDGE_LAN);
    TEST_ASSERT(eid >= 0, "edge added");
    TEST_ASSERT_EQ(g->edge_count, 1, "edge count");
    TEST_ASSERT(ri_graph_has_edge(g, 0, 1), "has edge 0->1");
    TEST_ASSERT(ri_graph_has_edge(g, 1, 0), "has edge 1->0 (undirected)");
    TEST_ASSERT(!ri_graph_has_edge(g, 0, 2), "no edge to nonexistent");
    ri_graph_destroy(g);
}

static void test_kruskal_mst(void)
{
    ri_graph_t *g = mock_build_sample_graph();
    int mst_edges = ri_graph_kruskal_mst(g);

    /* MST should have host_count-1 edges for connected graph */
    TEST_ASSERT_EQ(mst_edges, g->host_count - 1, "MST edge count");

    /* Verify all hosts are reachable via MST */
    int *parent = calloc((size_t)g->host_count, sizeof(int));
    int *depth = calloc((size_t)g->host_count, sizeof(int));
    ri_graph_bfs_mst(g, 0, parent, depth);

    int reachable = 0;
    for (int i = 0; i < g->host_count; i++) {
        if (depth[i] >= 0) reachable++;
    }
    TEST_ASSERT_EQ(reachable, g->host_count, "all hosts reachable via MST");

    free(parent);
    free(depth);
    ri_graph_destroy(g);
}

static void test_bfs_mst(void)
{
    ri_graph_t *g = mock_build_sample_graph();
    ri_graph_kruskal_mst(g);

    int *parent = calloc((size_t)g->host_count, sizeof(int));
    int *depth = calloc((size_t)g->host_count, sizeof(int));
    ri_graph_bfs_mst(g, 0, parent, depth);

    TEST_ASSERT_EQ(depth[0], 0, "root depth is 0");
    TEST_ASSERT(depth[1] > 0, "gateway has positive depth");
    TEST_ASSERT(depth[7] > 0, "target reachable");

    free(parent);
    free(depth);
    ri_graph_destroy(g);
}

void test_graph_suite(void)
{
    test_graph_create_destroy();
    test_graph_add_host();
    test_graph_find_by_ipv4();
    test_graph_add_edge();
    test_kruskal_mst();
    test_bfs_mst();
}
