#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <stddef.h>
#include "multiboot2.h"

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096ULL
#endif

void pmm_init(uint64_t multiboot_addr);
void *pmm_alloc_page(void);
void pmm_free_page(void *addr);
void *pmm_alloc_contiguous(size_t count);

uint64_t pmm_get_total_memory(void);
uint64_t pmm_get_used_memory(void);
uint64_t pmm_get_free_memory(void);

#endif
