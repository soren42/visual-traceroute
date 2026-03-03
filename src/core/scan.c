#include "core/scan.h"
#include "net/iface.h"
#include "net/ri_route.h"
#include "net/arp.h"
#include "net/dns.h"
#include "net/ping.h"
#include "net/icmp.h"
#include "net/icmp6.h"
#include "net/mdns.h"
#include "log.h"
#include "util/alloc.h"
#include "util/strutil.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <arpa/inet.h>

/* ---------- progress callback ------------------------------------------ */

static ri_progress_fn g_progress_fn;
static void          *g_progress_ctx;

void ri_scan_set_progress(ri_progress_fn fn, void *ctx)
{
    g_progress_fn  = fn;
    g_progress_ctx = ctx;
}

static void scan_progress(const char *fmt, ...)
{
    if (!g_progress_fn) return;
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    g_progress_fn(buf, g_progress_ctx);
}

/* ----------------------------------------------------------------------- */

int ri_scan_local_interfaces(ri_graph_t *g, const ri_config_t *cfg)
{
    (void)cfg;
    LOG_INFO("Phase 1: Local host identification");
    scan_progress("Enumerating local interfaces...");
    int n = ri_iface_enumerate(g);
    if (n < 0) {
        LOG_ERROR("Failed to enumerate network interfaces");
        return -1;
    }
    LOG_INFO("Phase 1: discovered %d local interface(s)", n);
    scan_progress("Found %d local interface(s)", n);
    return n;
}

int ri_scan_routing_table(ri_graph_t *g, const ri_config_t *cfg)
{
    (void)cfg;
    LOG_INFO("Phase 2: Routing table analysis");
    scan_progress("Reading routing table...");
    int n = ri_route_read(g);
    if (n < 0) {
        LOG_WARN("Failed to read routing table");
        return -1;
    }
    LOG_INFO("Phase 2: discovered %d route(s)", n);
    scan_progress("Found %d route(s)", n);
    return n;
}

int ri_scan_lan_discovery(ri_graph_t *g, const ri_config_t *cfg)
{
    LOG_INFO("Phase 3: LAN discovery");
    scan_progress("LAN discovery...");

    if (!cfg->no_arp) {
        int n = ri_arp_read(g);
        if (n < 0) {
            LOG_WARN("Failed to read ARP cache");
        } else {
            LOG_INFO("Phase 3: %d ARP cache entries", n);
            scan_progress("ARP cache: %d entries", n);
        }
    }

    if (cfg->subnet_scan) {
        LOG_INFO("Phase 3: active subnet scan");
        scan_progress("Subnet ping sweep (this may take a while)...");
        for (int i = 0; i < g->host_count; i++) {
            ri_host_t *h = &g->hosts[i];
            if (h->type != RI_HOST_LOCAL) continue;
            /* Scan primary IPv4 subnet */
            if (h->has_ipv4) {
                struct in_addr base;
                base.s_addr = h->ipv4.s_addr & htonl(0xFFFFFF00);
                int n = ri_ping_sweep(g, base, 24, RI_PING_TIMEOUT_MS);
                if (n > 0) {
                    LOG_INFO("Subnet scan: %d hosts on %s", n, h->iface_name);
                    scan_progress("Subnet scan: %d hosts on %s", n, h->iface_name);
                }
            }
            /* Scan secondary IPv4 subnets */
            for (int j = 0; j < h->ipv4_count; j++) {
                struct in_addr base;
                base.s_addr = h->ipv4_addrs[j].s_addr & htonl(0xFFFFFF00);
                int n = ri_ping_sweep(g, base, 24, RI_PING_TIMEOUT_MS);
                if (n > 0) {
                    LOG_INFO("Subnet scan: %d hosts", n);
                    scan_progress("Subnet scan: %d more hosts", n);
                }
            }
        }
    }

    LOG_INFO("Phase 3 complete");
    return 0;
}

int ri_scan_name_resolution(ri_graph_t *g, const ri_config_t *cfg)
{
    LOG_INFO("Phase 4: Name resolution");
    scan_progress("Resolving hostnames...");

    int n = ri_dns_resolve_all(g, cfg->nameserver);
    if (n < 0) {
        LOG_WARN("DNS resolution errors");
    } else {
        LOG_INFO("Phase 4: resolved %d hostname(s)", n);
        scan_progress("Resolved %d hostname(s)", n);
    }

    if (!cfg->no_mdns) {
        LOG_INFO("Phase 4: mDNS browse (3s timeout)");
        scan_progress("mDNS discovery (3s)...");
        int m = ri_mdns_browse(g, 3000);
        if (m < 0) {
            LOG_WARN("mDNS discovery failed");
        } else {
            LOG_INFO("Phase 4: %d mDNS name(s)", m);
            scan_progress("mDNS: %d name(s)", m);
        }
    }

    LOG_INFO("Phase 4 complete");
    return 0;
}

/* ---------- BFS expansion around a timeout gap ------------------------- */

static void expand_around_hop(ri_graph_t *g, int start_id, int max_depth)
{
    /* BFS queue: array of host IDs at current and next levels */
    int queue_cap = 64;
    int *cur_queue  = malloc((size_t)queue_cap * sizeof(int));
    int *next_queue = malloc((size_t)queue_cap * sizeof(int));
    if (!cur_queue || !next_queue) {
        free(cur_queue);
        free(next_queue);
        return;
    }

    /* Track scanned subnets to avoid re-scanning */
    int subnets_cap = 64;
    int subnets_count = 0;
    uint32_t *scanned_subnets = malloc((size_t)subnets_cap * sizeof(uint32_t));
    if (!scanned_subnets) {
        free(cur_queue);
        free(next_queue);
        return;
    }

    int cur_count = 1;
    int next_count = 0;
    cur_queue[0] = start_id;

    for (int depth = 0; depth < max_depth; depth++) {
        next_count = 0;

        for (int qi = 0; qi < cur_count; qi++) {
            int host_id = cur_queue[qi];
            if (!g->hosts[host_id].has_ipv4) continue;

            uint32_t net = ntohl(g->hosts[host_id].ipv4.s_addr) & 0xFFFFFF00;

            /* Check if already scanned this subnet */
            int already = 0;
            for (int s = 0; s < subnets_count; s++) {
                if (scanned_subnets[s] == net) { already = 1; break; }
            }
            if (already) continue;

            /* Record this subnet */
            if (subnets_count >= subnets_cap) {
                subnets_cap *= 2;
                uint32_t *tmp = realloc(scanned_subnets,
                                        (size_t)subnets_cap * sizeof(uint32_t));
                if (!tmp) goto done;
                scanned_subnets = tmp;
            }
            scanned_subnets[subnets_count++] = net;

            LOG_INFO("Hop expand: depth %d, probing %d.%d.%d.0/24",
                     depth + 1,
                     (net >> 24) & 0xFF, (net >> 16) & 0xFF,
                     (net >> 8) & 0xFF);
            scan_progress("Hop expand: depth %d, probing %d.%d.%d.0/24...",
                          depth + 1,
                          (net >> 24) & 0xFF, (net >> 16) & 0xFF,
                          (net >> 8) & 0xFF);

            int scan_count = 0;
            for (uint32_t h = 1; h < 255; h++) {
                struct in_addr probe;
                probe.s_addr = htonl(net | h);

                int existing = ri_graph_find_by_ipv4(g, probe);
                if (existing >= 0) continue;

                double probe_rtt = ri_icmp_ping(probe, 500);
                if (probe_rtt < 0) continue;

                ri_host_t ph;
                ri_host_init(&ph);
                ri_host_set_ipv4_addr(&ph, probe);
                ph.type = RI_HOST_REMOTE;
                ph.rtt_ms = probe_rtt;
                int pid = ri_graph_add_host(g, &ph);
                ri_graph_add_edge(g, host_id, pid,
                                  probe_rtt > 0 ? probe_rtt : 1.0,
                                  RI_EDGE_LAN);
                scan_count++;

                /* Enqueue newly discovered host for next BFS level */
                if (next_count >= queue_cap) {
                    queue_cap *= 2;
                    int *tmp = realloc(next_queue,
                                       (size_t)queue_cap * sizeof(int));
                    if (!tmp) goto done;
                    next_queue = tmp;
                    tmp = realloc(cur_queue,
                                  (size_t)queue_cap * sizeof(int));
                    if (!tmp) goto done;
                    cur_queue = tmp;
                }
                next_queue[next_count++] = pid;
            }

            if (scan_count > 0) {
                LOG_INFO("Hop expand: %d neighbor(s) at depth %d",
                         scan_count, depth + 1);
                scan_progress("Hop expand: %d neighbor(s) at depth %d",
                              scan_count, depth + 1);
            }
        }

        if (next_count == 0) break;

        /* Swap queues for next level */
        int *tmp = cur_queue;
        cur_queue = next_queue;
        next_queue = tmp;
        cur_count = next_count;
    }

done:
    free(cur_queue);
    free(next_queue);
    free(scanned_subnets);
}

/* ----------------------------------------------------------------------- */

int ri_scan_traceroute(ri_graph_t *g, const ri_config_t *cfg)
{
    if (!cfg->has_target) {
        LOG_INFO("Phase 5: skipped (no target)");
        scan_progress("Traceroute: skipped (no target)");
        return 0;
    }

    LOG_INFO("Phase 5: Traceroute to %s", cfg->target);
    scan_progress("Traceroute to %s...", cfg->target);

    struct in_addr target_addr;
    if (inet_pton(AF_INET, cfg->target, &target_addr) != 1) {
        LOG_ERROR("Invalid target IP: %s", cfg->target);
        return -1;
    }

    int prev_id = -1;
    int max_ttl = RI_MAX_HOPS;

    /* Find local host as starting point */
    for (int i = 0; i < g->host_count; i++) {
        if (g->hosts[i].type == RI_HOST_LOCAL) {
            prev_id = i;
            break;
        }
    }

    for (int ttl = 1; ttl <= max_ttl; ttl++) {
        struct in_addr reply_addr;
        double rtt = 0;
        int rc = ri_icmp_probe(target_addr, ttl, RI_PING_TIMEOUT_MS,
                               &reply_addr, &rtt);
        if (rc < 0) {
            LOG_DEBUG("TTL %d: no response", ttl);
            /* Expand around the last known-good hop on timeout */
            if (cfg->hop_scan && prev_id >= 0 && cfg->max_depth > 0)
                expand_around_hop(g, prev_id, cfg->max_depth);
            continue;
        }

        /* Check if hop already in graph */
        int hop_id = ri_graph_find_by_ipv4(g, reply_addr);
        if (hop_id < 0) {
            ri_host_t h;
            ri_host_init(&h);
            ri_host_set_ipv4_addr(&h, reply_addr);
            h.type = RI_HOST_REMOTE;
            h.hop_distance = ttl;
            h.rtt_ms = rtt;
            hop_id = ri_graph_add_host(g, &h);
        } else {
            if (g->hosts[hop_id].rtt_ms < 0)
                g->hosts[hop_id].rtt_ms = rtt;
        }

        LOG_DEBUG("TTL %d: %s (%.1f ms)", ttl,
                  ri_host_ipv4_str(&g->hosts[hop_id]), rtt);
        scan_progress("TTL %d: %s (%.1f ms)", ttl,
                      ri_host_ipv4_str(&g->hosts[hop_id]), rtt);

        /* Add edge from previous hop */
        if (prev_id >= 0 && !ri_graph_has_edge(g, prev_id, hop_id)) {
            double w = (rtt > 0) ? rtt : 1.0;
            ri_graph_add_edge(g, prev_id, hop_id, w, RI_EDGE_ROUTE);
        }

        /* Check if reached target */
        if (reply_addr.s_addr == target_addr.s_addr) {
            g->hosts[hop_id].type = RI_HOST_TARGET;
            LOG_INFO("Reached target in %d hops", ttl);
            break;
        }

        prev_id = hop_id;
    }

    LOG_INFO("Phase 5 complete");
    return 0;
}

int ri_scan_ipv6_augment(ri_graph_t *g, const ri_config_t *cfg)
{
    if (cfg->ipv4_only) {
        LOG_INFO("Phase 6: skipped (IPv4-only mode)");
        scan_progress("IPv6 discovery: skipped (IPv4-only)");
        return 0;
    }

    LOG_INFO("Phase 6: IPv6 neighbor discovery");
    scan_progress("IPv6 neighbor discovery...");
    int total = 0;

    for (int i = 0; i < g->host_count; i++) {
        ri_host_t *h = &g->hosts[i];
        if (h->type != RI_HOST_LOCAL) continue;
        if (ri_str_empty(h->interfaces)) continue;

        /* Parse comma-separated interfaces list */
        char buf[RI_IFACES_LEN];
        ri_strlcpy(buf, h->interfaces, sizeof(buf));
        char *saveptr = NULL;
        char *tok = strtok_r(buf, ",", &saveptr);
        while (tok) {
            int n = ri_icmp6_discover_neighbors(tok, g);
            if (n > 0) {
                LOG_INFO("IPv6: %d neighbor(s) on %s", n, tok);
                total += n;
            }
            tok = strtok_r(NULL, ",", &saveptr);
        }
    }

    LOG_INFO("Phase 6: %d total IPv6 neighbor(s)", total);
    scan_progress("IPv6: %d neighbor(s)", total);
    return total;
}

ri_graph_t *ri_scan_run(const ri_config_t *cfg)
{
    if (!cfg) {
        LOG_ERROR("NULL configuration");
        return NULL;
    }

    LOG_INFO("Starting network discovery");

    ri_graph_t *g = ri_graph_create();
    if (!g) {
        LOG_ERROR("Failed to create graph");
        return NULL;
    }

    if (ri_scan_local_interfaces(g, cfg) < 0) {
        LOG_ERROR("Phase 1 failed - cannot continue");
        ri_graph_destroy(g);
        return NULL;
    }

    ri_scan_routing_table(g, cfg);
    ri_scan_lan_discovery(g, cfg);
    ri_scan_name_resolution(g, cfg);
    ri_scan_traceroute(g, cfg);
    ri_scan_ipv6_augment(g, cfg);

    /* Compute display names for all hosts */
    for (int i = 0; i < g->host_count; i++) {
        ri_host_compute_display_name(&g->hosts[i]);
    }

    LOG_INFO("Discovery complete: %d hosts, %d edges",
             g->host_count, g->edge_count);
    scan_progress("Discovery complete: %d hosts, %d edges",
                  g->host_count, g->edge_count);
    return g;
}
