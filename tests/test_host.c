#include "core/host.h"
#include "util/strutil.h"
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
#define TEST_ASSERT_STR(a, b, msg) TEST_ASSERT(strcmp((a),(b)) == 0, msg)

static void test_host_init(void)
{
    ri_host_t h;
    ri_host_init(&h);
    TEST_ASSERT(h.id == -1, "init id is -1");
    TEST_ASSERT(h.type == RI_HOST_LAN, "init type is LAN");
    TEST_ASSERT(h.hop_distance == -1, "init hop is -1");
    TEST_ASSERT(!h.has_ipv4, "init has no ipv4");
    TEST_ASSERT(!h.has_ipv6, "init has no ipv6");
    TEST_ASSERT(!h.has_mac, "init has no mac");
}

static void test_host_set_ipv4(void)
{
    ri_host_t h;
    ri_host_init(&h);
    int rc = ri_host_set_ipv4(&h, "192.168.1.1");
    TEST_ASSERT(rc == 0, "set ipv4 ok");
    TEST_ASSERT(h.has_ipv4, "has ipv4 flag set");
    TEST_ASSERT_STR(ri_host_ipv4_str(&h), "192.168.1.1", "ipv4 string");

    rc = ri_host_set_ipv4(&h, "invalid");
    TEST_ASSERT(rc != 0, "invalid ipv4 fails");
}

static void test_host_display_name(void)
{
    ri_host_t h;

    /* DNS takes top priority */
    ri_host_init(&h);
    ri_host_set_ipv4(&h, "10.0.0.1");
    ri_strlcpy(h.hostname, "myhost", sizeof(h.hostname));
    ri_strlcpy(h.dns_name, "myhost.local", sizeof(h.dns_name));
    ri_host_compute_display_name(&h);
    TEST_ASSERT_STR(h.display_name, "myhost.local", "dns priority over hostname");

    /* Hostname used if no DNS (and not an iface name) */
    ri_host_init(&h);
    ri_host_set_ipv4(&h, "10.0.0.1");
    ri_strlcpy(h.hostname, "myhost", sizeof(h.hostname));
    ri_host_compute_display_name(&h);
    TEST_ASSERT_STR(h.display_name, "myhost", "hostname when no dns");

    /* Interface-like hostname skipped in favor of IP */
    ri_host_init(&h);
    ri_host_set_ipv4(&h, "10.0.0.1");
    ri_strlcpy(h.hostname, "en0", sizeof(h.hostname));
    ri_host_compute_display_name(&h);
    TEST_ASSERT_STR(h.display_name, "10.0.0.1", "iface hostname skipped");

    /* mDNS if no hostname/dns, with unescape */
    ri_host_init(&h);
    ri_host_set_ipv4(&h, "10.0.0.2");
    ri_strlcpy(h.mdns_name, "my\\032printer._http._tcp", sizeof(h.mdns_name));
    ri_host_compute_display_name(&h);
    TEST_ASSERT_STR(h.display_name, "my printer._http._tcp", "mdns unescaped");

    /* DNS if no hostname or mDNS */
    ri_host_init(&h);
    ri_host_set_ipv4(&h, "10.0.0.3");
    ri_strlcpy(h.dns_name, "server.example.com", sizeof(h.dns_name));
    ri_host_compute_display_name(&h);
    TEST_ASSERT_STR(h.display_name, "server.example.com", "dns priority");

    /* IP if no names */
    ri_host_init(&h);
    ri_host_set_ipv4(&h, "10.0.0.4");
    ri_host_compute_display_name(&h);
    TEST_ASSERT_STR(h.display_name, "10.0.0.4", "ip fallback");

    /* Unknown if nothing */
    ri_host_init(&h);
    ri_host_compute_display_name(&h);
    TEST_ASSERT_STR(h.display_name, "(unknown)", "unknown fallback");
}

static void test_host_type_str(void)
{
    TEST_ASSERT_STR(ri_host_type_str(RI_HOST_LOCAL), "local", "type local");
    TEST_ASSERT_STR(ri_host_type_str(RI_HOST_GATEWAY), "gateway", "type gw");
    TEST_ASSERT_STR(ri_host_type_str(RI_HOST_TARGET), "target", "type target");
}

void test_host_suite(void)
{
    test_host_init();
    test_host_set_ipv4();
    test_host_display_name();
    test_host_type_str();
}
