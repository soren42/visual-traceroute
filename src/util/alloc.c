#include "util/alloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void *ri_malloc(size_t size)
{
    void *p = malloc(size);
    if (!p && size > 0) {
        fprintf(stderr, "ri_malloc: out of memory (%zu bytes)\n", size);
        abort();
    }
    return p;
}

void *ri_calloc(size_t count, size_t size)
{
    void *p = calloc(count, size);
    if (!p && count > 0 && size > 0) {
        fprintf(stderr, "ri_calloc: out of memory (%zu * %zu bytes)\n",
                count, size);
        abort();
    }
    return p;
}

void *ri_realloc(void *ptr, size_t size)
{
    void *p = realloc(ptr, size);
    if (!p && size > 0) {
        fprintf(stderr, "ri_realloc: out of memory (%zu bytes)\n", size);
        abort();
    }
    return p;
}

char *ri_strdup(const char *s)
{
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char *p = ri_malloc(len);
    memcpy(p, s, len);
    return p;
}

void ri_free(void *ptr)
{
    free(ptr);
}
