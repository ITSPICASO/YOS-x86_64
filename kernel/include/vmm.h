#ifndef VMM_H
#define VMM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define PAGE_PRESENT  (1ULL << 0)
#define PAGE_WRITABLE (1ULL << 1)
#define PAGE_USER     (1ULL << 2)
#define PAGE_COW      (1ULL << 9) /* Bit disponible pour l'OS */

typedef struct {
    uint64_t entries[512];
} page_table_t;

extern page_table_t *kernel_pml4;

void vmm_init(void);
void vmm_map_page(page_table_t *pml4, uint64_t virt, uint64_t phys, uint64_t flags);
void vmm_unmap_page(page_table_t *pml4, uint64_t virt);
page_table_t *vmm_clone_address_space(page_table_t *src_pml4, uint64_t *out_phys);
void vmm_destroy_address_space(page_table_t *pml4);
int vmm_handle_cow(uint64_t fault_addr);

#endif
