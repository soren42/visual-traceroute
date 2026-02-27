#include "config.h"
#ifdef RI_LINUX

#include "net/mdns.h"
#include "log.h"
#include "util/alloc.h"
#include "util/strutil.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>

#ifdef HAVE_AVAHI
#include <avahi-client/client.h>
#include <avahi-client/lookup.h>
#include <avahi-common/simple-watch.h>
#include <avahi-common/error.h>

/* Avahi-based implementation would go here */
/* For now, fall through to avahi-browse fallback */
#endif

int ri_mdns_browse(ri_graph_t *g, int timeout_ms)
{
    (void)timeout_ms;

    /* Fallback: try avahi-browse command */
    FILE *fp = popen("avahi-browse -apt 2>/dev/null", "r");
    if (!fp) {
        LOG_DEBUG("mDNS: avahi-browse not available");
        return 0;
    }

    char line[512];
    int count = 0;

    while (fgets(line, sizeof(line), fp)) {
        /* Parse avahi-browse output: +;iface;proto;name;type;domain */
        if (line[0] != '+') continue;

        char *fields[6];
        int nf = 0;
        char *p = line;
        while (nf < 6 && p) {
            fields[nf++] = p;
            p = strchr(p, ';');
            if (p) *p++ = '\0';
        }
        if (nf < 5) continue;

        /* fields[3] = service name */
        LOG_DEBUG("mDNS: found service %s", fields[3]);

        /* Try to match to existing hosts */
        for (int i = 0; i < g->host_count; i++) {
            if (ri_str_empty(g->hosts[i].mdns_name)) {
                ri_strlcpy(g->hosts[i].mdns_name, fields[3],
                           sizeof(g->hosts[i].mdns_name));
                count++;
                break;
            }
        }
    }

    pclose(fp);
    return count;
}

#endif /* RI_LINUX */
