#include "output/out_json.h"
#include "core/json_out.h"
#include "log.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>

int ri_out_json(const ri_graph_t *g, const char *filename)
{
    cJSON *root = ri_json_serialize(g);
    if (!root) {
        LOG_ERROR("Failed to serialize graph to JSON");
        return -1;
    }

    char *str = cJSON_Print(root);
    cJSON_Delete(root);
    if (!str) {
        LOG_ERROR("Failed to print JSON");
        return -1;
    }

    FILE *fp = fopen(filename, "w");
    if (!fp) {
        LOG_ERROR("Cannot open %s for writing", filename);
        free(str);
        return -1;
    }

    fprintf(fp, "%s\n", str);
    fclose(fp);
    free(str);

    LOG_INFO("Wrote JSON to %s", filename);
    return 0;
}
