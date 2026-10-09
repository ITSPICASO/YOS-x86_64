#ifndef SLAB_H
#define SLAB_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define SLAB_MAX_CACHES 16

typedef struct slab_cache {
    const char *name;
    size_t      obj_size;
    size_t      objs_per_slab;
    void       *free_list;
    size_t      total_objs;
    size_t      free_objs;
} slab_cache_t;

void slab_init(void);
slab_cache_t *slab_cache_create(const char *name, size_t obj_size);
void *slab_alloc(slab_cache_t *cache);
void slab_free(slab_cache_t *cache, void *obj);

/* Etape 33 : Guard Pages */
void guard_page_create(uint64_t virt_addr);

#endif
