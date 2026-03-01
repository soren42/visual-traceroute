#include "cli.h"
#include <stdio.h>
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
#define TEST_ASSERT_STR(a, b, msg) TEST_ASSERT(strcmp((a),(b)) == 0, msg)

static void test_cli_defaults(void)
{
    ri_config_t cfg;
    ri_cli_defaults(&cfg);
    TEST_ASSERT_EQ(cfg.max_depth, 1, "default depth is 1");
    TEST_ASSERT_EQ(cfg.verbosity, 0, "default verbosity is 0");
    TEST_ASSERT_EQ(cfg.output_flags, (unsigned)RI_OUT_JSON, "default output json");
    TEST_ASSERT_STR(cfg.file_base, "network", "default file base");
    TEST_ASSERT(!cfg.has_target, "no target by default");
    TEST_ASSERT(!cfg.ipv4_only, "not ipv4 only");
    TEST_ASSERT(!cfg.ipv6_only, "not ipv6 only");
    TEST_ASSERT(!cfg.no_mdns, "mdns enabled");
    TEST_ASSERT(!cfg.no_arp, "arp enabled");
    TEST_ASSERT(!cfg.subnet_scan, "no subnet scan");
}

static void test_cli_target(void)
{
    ri_config_t cfg;
    char *argv[] = {"prog", "-t", "8.8.8.8"};
    int rc = ri_cli_parse(&cfg, 3, argv);
    TEST_ASSERT_EQ(rc, 0, "parse target ok");
    TEST_ASSERT(cfg.has_target, "has target");
    TEST_ASSERT_STR(cfg.target, "8.8.8.8", "target value");
}

static void test_cli_verbosity(void)
{
    ri_config_t cfg;
    char *argv[] = {"prog", "-vvv"};
    int rc = ri_cli_parse(&cfg, 2, argv);
    TEST_ASSERT_EQ(rc, 0, "parse -vvv ok");
    TEST_ASSERT_EQ(cfg.verbosity, 3, "verbosity 3");
}

static void test_cli_output_formats(void)
{
    ri_config_t cfg;
    char *argv[] = {"prog", "-o", "json,png,html"};
    int rc = ri_cli_parse(&cfg, 3, argv);
    TEST_ASSERT_EQ(rc, 0, "parse output ok");
    TEST_ASSERT(cfg.output_flags & RI_OUT_JSON, "json flag");
    TEST_ASSERT(cfg.output_flags & RI_OUT_PNG, "png flag");
    TEST_ASSERT(cfg.output_flags & RI_OUT_HTML, "html flag");
    TEST_ASSERT(!(cfg.output_flags & RI_OUT_CURSES), "no curses flag");
}

static void test_cli_depth(void)
{
    ri_config_t cfg;
    char *argv[] = {"prog", "-d", "5"};
    int rc = ri_cli_parse(&cfg, 3, argv);
    TEST_ASSERT_EQ(rc, 0, "parse depth ok");
    TEST_ASSERT_EQ(cfg.max_depth, 5, "depth 5");
}

static void test_cli_file(void)
{
    ri_config_t cfg;
    char *argv[] = {"prog", "-f", "output"};
    int rc = ri_cli_parse(&cfg, 3, argv);
    TEST_ASSERT_EQ(rc, 0, "parse file ok");
    TEST_ASSERT_STR(cfg.file_base, "output", "file base");
}

static void test_cli_ipv4_only(void)
{
    ri_config_t cfg;
    char *argv[] = {"prog", "-4"};
    int rc = ri_cli_parse(&cfg, 2, argv);
    TEST_ASSERT_EQ(rc, 0, "parse -4 ok");
    TEST_ASSERT(cfg.ipv4_only, "ipv4 only set");
    TEST_ASSERT(!cfg.ipv6_only, "ipv6 only not set");
}

static void test_cli_nameserver(void)
{
    ri_config_t cfg;
    char *argv[] = {"prog", "-n", "10.0.0.1"};
    int rc = ri_cli_parse(&cfg, 3, argv);
    TEST_ASSERT_EQ(rc, 0, "parse nameserver ok");
    TEST_ASSERT_STR(cfg.nameserver, "10.0.0.1", "nameserver value");
}

static void test_cli_nameserver_long(void)
{
    ri_config_t cfg;
    char *argv[] = {"prog", "--nameserver", "192.168.1.53"};
    int rc = ri_cli_parse(&cfg, 3, argv);
    TEST_ASSERT_EQ(rc, 0, "parse --nameserver ok");
    TEST_ASSERT_STR(cfg.nameserver, "192.168.1.53", "nameserver long value");
}

static void test_cli_combined(void)
{
    ri_config_t cfg;
    char *argv[] = {"prog", "-t", "1.1.1.1", "-d", "3", "-vv",
                    "-o", "json,curses", "-f", "test"};
    int rc = ri_cli_parse(&cfg, 10, argv);
    TEST_ASSERT_EQ(rc, 0, "combined parse ok");
    TEST_ASSERT(cfg.has_target, "has target");
    TEST_ASSERT_STR(cfg.target, "1.1.1.1", "target");
    TEST_ASSERT_EQ(cfg.max_depth, 3, "depth 3");
    TEST_ASSERT_EQ(cfg.verbosity, 2, "verbosity 2");
    TEST_ASSERT(cfg.output_flags & RI_OUT_JSON, "json output");
    TEST_ASSERT(cfg.output_flags & RI_OUT_CURSES, "curses output");
    TEST_ASSERT_STR(cfg.file_base, "test", "file base");
}

void test_cli_suite(void)
{
    test_cli_defaults();
    test_cli_target();
    test_cli_verbosity();
    test_cli_output_formats();
    test_cli_depth();
    test_cli_file();
    test_cli_ipv4_only();
    test_cli_nameserver();
    test_cli_nameserver_long();
    test_cli_combined();
}
