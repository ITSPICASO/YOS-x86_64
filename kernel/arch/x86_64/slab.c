#include "slab.h"
#include "heap.h"
#include "vmm.h"
#include "serial.h"

static slab_cache_t caches[SLAB_MAX_CACHES];
static size_t cache_count = 0;

void slab_init(void) {
    serial_print("[*] Etape 32 : Initialisation du Slab Allocator...\n");
    cache_count = 0;
    for (size_t i = 0; i < SLAB_MAX_CACHES; i++) {
        caches[i].name = NULL;
        caches[i].obj_size = 0;
        caches[i].free_list = NULL;
    }
    serial_print("[+] Slab Allocator pret.\n");
}

slab_cache_t *slab_cache_create(const char *name, size_t obj_size) {
    if (cache_count >= SLAB_MAX_CACHES) return NULL;
    if (obj_size < sizeof(void *)) obj_size = sizeof(void *);

    slab_cache_t *c = &caches[cache_count++];
    c->name = name;
    c->obj_size = obj_size;
    c->objs_per_slab = 4096 / obj_size;
    c->free_list = NULL;
    c->total_objs = 0;
    c->free_objs = 0;

    serial_print("[+] Slab Cache cree: ");
    serial_print(name);
    serial_print(" (Taille objet: ");
    serial_print_dec((uint32_t)obj_size);
    serial_print(" octets)\n");

    return c;
}

void *slab_alloc(slab_cache_t *cache) {
    if (!cache) return NULL;

    /* S'il n'y a plus d'objets libres, allouer un nouveau bloc (slab) */
    if (!cache->free_list) {
        uint8_t *slab_mem = (uint8_t *)kmalloc(4096);
        if (!slab_mem) return NULL;

        /* Chainer tous les objets dans la free_list */
        for (size_t i = 0; i < cache->objs_per_slab; i++) {
            void *obj = (void *)(slab_mem + (i * cache->obj_size));
            *(void **)obj = cache->free_list;
            cache->free_list = obj;
            cache->total_objs++;
            cache->free_objs++;
        }
    }

    /* Depiler le premier objet libre */
    void *allocated = cache->free_list;
    cache->free_list = *(void **)allocated;
    cache->free_objs--;

    return allocated;
}

void slab_free(slab_cache_t *cache, void *obj) {
    if (!cache || !obj) return;

    /* Empiler l'objet libere dans la free_list */
    *(void **)obj = cache->free_list;
    cache->free_list = obj;
    cache->free_objs++;
}

/* Etape 33 : Detection des depassements de pile via Guard Pages */
void guard_page_create(uint64_t virt_addr) {
    /* Demapper la page pour declencher #PF en cas de collision */
    vmm_unmap_page(kernel_pml4, virt_addr);
    serial_print("[+] Etape 33 : Guard Page configuree a 0x");
    serial_print_hex(virt_addr);
    serial_print(" (Acces protege contre le debordement)\n");
}
