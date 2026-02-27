#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Minimal test framework */
int g_tests_run = 0;
int g_tests_passed = 0;
int g_tests_failed = 0;

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
#define TEST_ASSERT_NE(a, b, msg) TEST_ASSERT((a) != (b), msg)
#define TEST_ASSERT_STR(a, b, msg) TEST_ASSERT(strcmp((a),(b)) == 0, msg)

#define RUN_SUITE(name) do { \
    printf("Suite: %s\n", #name); \
    name(); \
} while(0)

/* Test suites - declared in other files */
extern void test_graph_suite(void);
extern void test_host_suite(void);
extern void test_layout_suite(void);
extern void test_json_suite(void);
extern void test_cli_suite(void);

int main(void)
{
    printf("RouteInspection Test Runner\n");
    printf("===========================\n\n");

    RUN_SUITE(test_graph_suite);
    RUN_SUITE(test_host_suite);
    RUN_SUITE(test_layout_suite);
    RUN_SUITE(test_json_suite);
    RUN_SUITE(test_cli_suite);

    printf("\n===========================\n");
    printf("Results: %d/%d passed, %d failed\n",
           g_tests_passed, g_tests_run, g_tests_failed);

    return g_tests_failed > 0 ? 1 : 0;
}
