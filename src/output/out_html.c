#include "output/out_html.h"
#include "output/threejs_template.h"
#include "core/json_out.h"
#include "log.h"
#include "cJSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int ri_out_html(const ri_graph_t *g, const char *filename)
{
    cJSON *json = ri_json_serialize(g);
    if (!json) {
        LOG_ERROR("Failed to serialize graph for HTML");
        return -1;
    }

    char *json_str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    if (!json_str) {
        LOG_ERROR("Failed to print JSON for HTML");
        return -1;
    }

    FILE *fp = fopen(filename, "w");
    if (!fp) {
        LOG_ERROR("Cannot open %s", filename);
        free(json_str);
        return -1;
    }

    /* Write template, replacing the placeholder with actual data */
    const char *placeholder = "/* GRAPH_DATA_PLACEHOLDER */";
    const char *pos = strstr(THREEJS_HTML_TEMPLATE, placeholder);

    if (pos) {
        /* Write everything before placeholder */
        fwrite(THREEJS_HTML_TEMPLATE, 1,
               (size_t)(pos - THREEJS_HTML_TEMPLATE), fp);
        /* Write the data assignment */
        fprintf(fp, "const graphData = %s;", json_str);
        /* Write everything after placeholder */
        fputs(pos + strlen(placeholder), fp);
    } else {
        /* No placeholder found, write template then inject data */
        fputs(THREEJS_HTML_TEMPLATE, fp);
    }

    fclose(fp);
    free(json_str);

    LOG_INFO("Wrote HTML to %s", filename);
    return 0;
}
