#include "core/json_out.h"
#include "util/strutil.h"
#include <stdio.h>

static cJSON *host_to_json(const ri_host_t *h)
{
    cJSON *obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(obj, "id", h->id);
    cJSON_AddStringToObject(obj, "display_name", h->display_name);
    cJSON_AddStringToObject(obj, "type", ri_host_type_str(h->type));
    cJSON_AddNumberToObject(obj, "hop_distance", h->hop_distance);

    if (h->rtt_ms >= 0)
        cJSON_AddNumberToObject(obj, "rtt_ms", h->rtt_ms);

    if (h->has_ipv4)
        cJSON_AddStringToObject(obj, "ipv4", ri_host_ipv4_str(h));

    /* Secondary IPv4 addresses */
    if (h->ipv4_count > 0) {
        cJSON *arr = cJSON_CreateArray();
        for (int i = 0; i < h->ipv4_count; i++) {
            char buf[RI_IPV4_STR_LEN];
            inet_ntop(AF_INET, &h->ipv4_addrs[i], buf, sizeof(buf));
            cJSON_AddItemToArray(arr, cJSON_CreateString(buf));
        }
        cJSON_AddItemToObject(obj, "ipv4_addrs", arr);
    }

    if (h->has_ipv6) {
        char buf[RI_IPV6_STR_LEN];
        inet_ntop(AF_INET6, &h->ipv6, buf, sizeof(buf));
        cJSON_AddStringToObject(obj, "ipv6", buf);
    }

    /* Secondary IPv6 addresses */
    if (h->ipv6_count > 0) {
        cJSON *arr = cJSON_CreateArray();
        for (int i = 0; i < h->ipv6_count; i++) {
            char buf[RI_IPV6_STR_LEN];
            inet_ntop(AF_INET6, &h->ipv6_addrs[i], buf, sizeof(buf));
            cJSON_AddItemToArray(arr, cJSON_CreateString(buf));
        }
        cJSON_AddItemToObject(obj, "ipv6_addrs", arr);
    }

    if (h->has_mac) {
        char mac[RI_MAC_STR_LEN];
        ri_mac_to_str(h->mac, mac, sizeof(mac));
        cJSON_AddStringToObject(obj, "mac", mac);
    }

    if (!ri_str_empty(h->hostname))
        cJSON_AddStringToObject(obj, "hostname", h->hostname);
    if (!ri_str_empty(h->mdns_name)) {
        /* Output unescaped mDNS name */
        char tmp[RI_HOSTNAME_LEN];
        ri_strlcpy(tmp, h->mdns_name, sizeof(tmp));
        ri_str_unescape_mdns(tmp);
        cJSON_AddStringToObject(obj, "mdns_name", tmp);
    }
    if (!ri_str_empty(h->dns_name))
        cJSON_AddStringToObject(obj, "dns_name", h->dns_name);
    if (!ri_str_empty(h->interfaces))
        cJSON_AddStringToObject(obj, "interfaces", h->interfaces);
    else if (!ri_str_empty(h->iface_name))
        cJSON_AddStringToObject(obj, "interface", h->iface_name);

    /* Layout coords */
    cJSON_AddNumberToObject(obj, "x", h->x);
    cJSON_AddNumberToObject(obj, "y", h->y);
    cJSON_AddNumberToObject(obj, "z", h->z);

    return obj;
}

static cJSON *edge_to_json(const ri_edge_t *e)
{
    cJSON *obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(obj, "src_id", e->src_id);
    cJSON_AddNumberToObject(obj, "dst_id", e->dst_id);
    cJSON_AddNumberToObject(obj, "weight", e->weight);
    cJSON_AddStringToObject(obj, "type", ri_edge_type_str(e->type));
    cJSON_AddBoolToObject(obj, "in_mst", e->in_mst);
    return obj;
}

cJSON *ri_json_serialize(const ri_graph_t *g)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "version", "1.0");
    cJSON_AddNumberToObject(root, "host_count", g->host_count);
    cJSON_AddNumberToObject(root, "edge_count", g->edge_count);

    cJSON *hosts = cJSON_CreateArray();
    for (int i = 0; i < g->host_count; i++) {
        cJSON_AddItemToArray(hosts, host_to_json(&g->hosts[i]));
    }
    cJSON_AddItemToObject(root, "hosts", hosts);

    cJSON *edges = cJSON_CreateArray();
    for (int i = 0; i < g->edge_count; i++) {
        cJSON_AddItemToArray(edges, edge_to_json(&g->edges[i]));
    }
    cJSON_AddItemToObject(root, "edges", edges);

    return root;
}
