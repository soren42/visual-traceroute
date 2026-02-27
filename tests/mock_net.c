#include "mock_net.h"
#include "core/host.h"
#include "core/edge.h"
#include "util/strutil.h"
#include <string.h>

ri_graph_t *mock_build_sample_graph(void)
{
    ri_graph_t *g = ri_graph_create();

    /* Host 0: Local */
    ri_host_t h;
    ri_host_init(&h);
    ri_host_set_ipv4(&h, "192.168.1.100");
    h.type = RI_HOST_LOCAL;
    h.hop_distance = 0;
    h.rtt_ms = 0;
    ri_strlcpy(h.hostname, "my-machine", sizeof(h.hostname));
    ri_strlcpy(h.iface_name, "en0", sizeof(h.iface_name));
    unsigned char mac0[] = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0x01};
    ri_host_set_mac(&h, mac0);
    ri_graph_add_host(g, &h); /* id=0 */

    /* Host 1: Gateway */
    ri_host_init(&h);
    ri_host_set_ipv4(&h, "192.168.1.1");
    h.type = RI_HOST_GATEWAY;
    h.hop_distance = 1;
    h.rtt_ms = 1.5;
    ri_strlcpy(h.hostname, "router", sizeof(h.hostname));
    unsigned char mac1[] = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0x02};
    ri_host_set_mac(&h, mac1);
    ri_graph_add_host(g, &h); /* id=1 */

    /* Host 2-4: LAN hosts */
    ri_host_init(&h);
    ri_host_set_ipv4(&h, "192.168.1.50");
    h.type = RI_HOST_LAN;
    h.rtt_ms = 0.5;
    ri_strlcpy(h.hostname, "printer", sizeof(h.hostname));
    ri_graph_add_host(g, &h); /* id=2 */

    ri_host_init(&h);
    ri_host_set_ipv4(&h, "192.168.1.51");
    h.type = RI_HOST_LAN;
    h.rtt_ms = 0.3;
    ri_strlcpy(h.mdns_name, "laptop._http._tcp.local", sizeof(h.mdns_name));
    ri_graph_add_host(g, &h); /* id=3 */

    ri_host_init(&h);
    ri_host_set_ipv4(&h, "192.168.1.52");
    h.type = RI_HOST_LAN;
    h.rtt_ms = 0.8;
    ri_graph_add_host(g, &h); /* id=4 */

    /* Host 5-6: Remote hops */
    ri_host_init(&h);
    ri_host_set_ipv4(&h, "10.0.0.1");
    h.type = RI_HOST_REMOTE;
    h.hop_distance = 2;
    h.rtt_ms = 5.0;
    ri_graph_add_host(g, &h); /* id=5 */

    ri_host_init(&h);
    ri_host_set_ipv4(&h, "172.16.0.1");
    h.type = RI_HOST_REMOTE;
    h.hop_distance = 3;
    h.rtt_ms = 15.0;
    ri_graph_add_host(g, &h); /* id=6 */

    /* Host 7: Target */
    ri_host_init(&h);
    ri_host_set_ipv4(&h, "8.8.8.8");
    h.type = RI_HOST_TARGET;
    h.hop_distance = 4;
    h.rtt_ms = 20.0;
    ri_strlcpy(h.dns_name, "dns.google", sizeof(h.dns_name));
    ri_graph_add_host(g, &h); /* id=7 */

    /* Compute display names */
    for (int i = 0; i < g->host_count; i++)
        ri_host_compute_display_name(&g->hosts[i]);

    /* Edges */
    ri_graph_add_edge(g, 0, 1, 1.5, RI_EDGE_GATEWAY);  /* local -> gateway */
    ri_graph_add_edge(g, 0, 2, 0.5, RI_EDGE_LAN);      /* local -> printer */
    ri_graph_add_edge(g, 0, 3, 0.3, RI_EDGE_LAN);      /* local -> laptop */
    ri_graph_add_edge(g, 0, 4, 0.8, RI_EDGE_LAN);      /* local -> host */
    ri_graph_add_edge(g, 1, 5, 5.0, RI_EDGE_ROUTE);    /* gw -> hop1 */
    ri_graph_add_edge(g, 5, 6, 10.0, RI_EDGE_ROUTE);   /* hop1 -> hop2 */
    ri_graph_add_edge(g, 6, 7, 5.0, RI_EDGE_ROUTE);    /* hop2 -> target */

    return g;
}
