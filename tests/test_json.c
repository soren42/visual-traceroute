#include "core/json_out.h"
#include "mock_net.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static void test_json_serialize(void)
{
    ri_graph_t *g = mock_build_sample_graph();
    cJSON *json = ri_json_serialize(g);
    TEST_ASSERT(json != NULL, "serialize returns non-null");

    cJSON *hosts = cJSON_GetObjectItem(json, "hosts");
    TEST_ASSERT(hosts != NULL, "has hosts array");
    TEST_ASSERT(cJSON_IsArray(hosts), "hosts is array");
    TEST_ASSERT_EQ(cJSON_GetArraySize(hosts), 8, "8 hosts in array");

    cJSON *edges = cJSON_GetObjectItem(json, "edges");
    TEST_ASSERT(edges != NULL, "has edges array");
    TEST_ASSERT(cJSON_IsArray(edges), "edges is array");
    TEST_ASSERT_EQ(cJSON_GetArraySize(edges), 7, "7 edges in array");

    /* Check first host */
    cJSON *h0 = cJSON_GetArrayItem(hosts, 0);
    cJSON *name = cJSON_GetObjectItem(h0, "display_name");
    TEST_ASSERT(name != NULL, "host 0 has display_name");
    TEST_ASSERT(strcmp(cJSON_GetStringValue(name), "my-machine") == 0,
                "host 0 display name");

    cJSON *type = cJSON_GetObjectItem(h0, "type");
    TEST_ASSERT(strcmp(cJSON_GetStringValue(type), "local") == 0,
                "host 0 type is local");

    cJSON_Delete(json);
    ri_graph_destroy(g);
}

static void test_json_roundtrip(void)
{
    ri_graph_t *g = mock_build_sample_graph();
    cJSON *json = ri_json_serialize(g);
    char *str = cJSON_Print(json);
    TEST_ASSERT(str != NULL, "print produces string");
    TEST_ASSERT(strlen(str) > 100, "json string is non-trivial");

    /* Parse it back */
    cJSON *parsed = cJSON_Parse(str);
    TEST_ASSERT(parsed != NULL, "can parse back");

    cJSON *hc = cJSON_GetObjectItem(parsed, "host_count");
    TEST_ASSERT_EQ((int)cJSON_GetNumberValue(hc), 8, "host_count round-trip");

    cJSON_Delete(parsed);
    free(str);
    cJSON_Delete(json);
    ri_graph_destroy(g);
}

void test_json_suite(void)
{
    test_json_serialize();
    test_json_roundtrip();
}
