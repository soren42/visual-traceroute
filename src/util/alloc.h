#ifndef RI_ALLOC_H
#define RI_ALLOC_H

#include <stddef.h>

/* Checked allocation - aborts on failure */
void *ri_malloc(size_t size);
void *ri_calloc(size_t count, size_t size);
void *ri_realloc(void *ptr, size_t size);
char *ri_strdup(const char *s);
void  ri_free(void *ptr);

#endif /* RI_ALLOC_H */
