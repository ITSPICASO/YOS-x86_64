#include "vmm.h"
#include "pmm.h"
#include "serial.h"

page_table_t *kernel_pml4 = NULL;

void vmm_init(void) {
    serial_print("[*] VMM : Creation du PML4 kernel dedie...\n");

    /* 1. Allouer PML4 */
    uint64_t pml4_phys = (uint64_t)pmm_alloc_page();
    kernel_pml4 = (page_table_t *)pml4_phys;

    for (int i = 0; i < 512; i++) {
        kernel_pml4->entries[i] = 0;
    }

    /* 2. Allouer PDPT */
    uint64_t pdpt_phys = (uint64_t)pmm_alloc_page();
    page_table_t *pdpt = (page_table_t *)pdpt_phys;
    for (int i = 0; i < 512; i++) pdpt->entries[i] = 0;

    /* PML4[0] (Identity) et PML4[511] (Higher-Half) */
    kernel_pml4->entries[0]   = pdpt_phys | PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;
    kernel_pml4->entries[511] = pdpt_phys | PAGE_PRESENT | PAGE_WRITABLE;

    /* 3. PD0 (0 a 1GB) */
    uint64_t pd0_phys = (uint64_t)pmm_alloc_page();
    page_table_t *pd0 = (page_table_t *)pd0_phys;
    for (int i = 0; i < 512; i++) pd0->entries[i] = 0;

    /* Mapper les premiers 1GB en 2MB pages */
    for (uint64_t i = 0; i < 512; i++) {
        pd0->entries[i] = (i * 0x200000ULL) | PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER | (1ULL << 7);
    }

    /* 4. PD3 (3GB a 4GB pour MMIO : LAPIC 0xFEE00000 et IOAPIC 0xFEC00000) */
    uint64_t pd3_phys = (uint64_t)pmm_alloc_page();
    page_table_t *pd3 = (page_table_t *)pd3_phys;
    for (int i = 0; i < 512; i++) pd3->entries[i] = 0;

    for (uint64_t i = 0; i < 512; i++) {
        pd3->entries[i] = (0xC0000000ULL + i * 0x200000ULL) | PAGE_PRESENT | PAGE_WRITABLE | (1ULL << 7);
    }

    /* Lier PD0 et PD3 dans PDPT */
    pdpt->entries[0]   = pd0_phys | PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;
    pdpt->entries[3]   = pd3_phys | PAGE_PRESENT | PAGE_WRITABLE;
    pdpt->entries[510] = pd0_phys | PAGE_PRESENT | PAGE_WRITABLE; /* Higher half link */

    /* Charger CR3 */
    __asm__ volatile("mov %0, %%cr3" : : "r"(pml4_phys) : "memory");
    serial_print("[+] VMM : Nouveau CR3 charge avec 1GB Identity, 1GB Higher-Half et MMIO 3-4GB!\n");
}

void vmm_map_page(page_table_t *pml4, uint64_t virt, uint64_t phys, uint64_t flags) {
    if (!pml4) pml4 = kernel_pml4;

    uint64_t pml4_idx = (virt >> 39) & 0x1FF;
    uint64_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint64_t pd_idx   = (virt >> 21) & 0x1FF;
    uint64_t pt_idx   = (virt >> 12) & 0x1FF;

    /* PML4 -> PDPT */
    if (!(pml4->entries[pml4_idx] & PAGE_PRESENT)) {
        uint64_t new_pdpt = (uint64_t)pmm_alloc_page();
        page_table_t *t = (page_table_t *)new_pdpt;
        for (int i = 0; i < 512; i++) t->entries[i] = 0;
        pml4->entries[pml4_idx] = new_pdpt | PAGE_PRESENT | PAGE_WRITABLE | (flags & PAGE_USER);
    } else if (flags & PAGE_USER) {
        pml4->entries[pml4_idx] |= PAGE_USER;
    }
    page_table_t *pdpt = (page_table_t *)(pml4->entries[pml4_idx] & ~0xFFFULL);

    /* PDPT -> PD */
    if (!(pdpt->entries[pdpt_idx] & PAGE_PRESENT)) {
        uint64_t new_pd = (uint64_t)pmm_alloc_page();
        page_table_t *t = (page_table_t *)new_pd;
        for (int i = 0; i < 512; i++) t->entries[i] = 0;
        pdpt->entries[pdpt_idx] = new_pd | PAGE_PRESENT | PAGE_WRITABLE | (flags & PAGE_USER);
    } else if (flags & PAGE_USER) {
        pdpt->entries[pdpt_idx] |= PAGE_USER;
    }
    page_table_t *pd = (page_table_t *)(pdpt->entries[pdpt_idx] & ~0xFFFULL);

    /* PD -> PT */
    if (!(pd->entries[pd_idx] & PAGE_PRESENT)) {
        uint64_t new_pt = (uint64_t)pmm_alloc_page();
        page_table_t *t = (page_table_t *)new_pt;
        for (int i = 0; i < 512; i++) t->entries[i] = 0;
        pd->entries[pd_idx] = new_pt | PAGE_PRESENT | PAGE_WRITABLE | (flags & PAGE_USER);
    } else if (pd->entries[pd_idx] & (1ULL << 7)) {
        /* Splitting de la page 2MB vers 512 pages de 4KB */
        uint64_t base_phys = pd->entries[pd_idx] & ~0x1FFFFFULL;
        uint64_t old_flags = pd->entries[pd_idx] & 0xFFF;
        old_flags &= ~(1ULL << 7); /* Enlever bit Huge Page */

        uint64_t new_pt = (uint64_t)pmm_alloc_page();
        page_table_t *t = (page_table_t *)new_pt;
        for (int i = 0; i < 512; i++) {
            t->entries[i] = (base_phys + (i * 4096)) | old_flags | (flags & PAGE_USER);
        }
        pd->entries[pd_idx] = new_pt | PAGE_PRESENT | PAGE_WRITABLE | (flags & PAGE_USER);
    } else if (flags & PAGE_USER) {
        pd->entries[pd_idx] |= PAGE_USER;
    }
    page_table_t *pt = (page_table_t *)(pd->entries[pd_idx] & ~0xFFFULL);

    /* PT -> Page physique */
    pt->entries[pt_idx] = (phys & ~0xFFFULL) | flags;

    __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

void vmm_unmap_page(page_table_t *pml4, uint64_t virt) {
    (void)pml4;
    __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

page_table_t *vmm_clone_address_space(page_table_t *src_pml4, uint64_t *out_phys) {
    if (!src_pml4) src_pml4 = kernel_pml4;

    uint64_t pml4_phys = (uint64_t)pmm_alloc_page();
    if (!pml4_phys) return NULL;

    page_table_t *new_pml4 = (page_table_t *)pml4_phys;

    /* Copier l'intégralité du PML4 pour conserver l'Identity/MMIO (Entry 0) et le Higher-Half */
    for (int i = 0; i < 512; i++) {
        new_pml4->entries[i] = src_pml4->entries[i];
    }

    if (out_phys) {
        *out_phys = pml4_phys;
    }

    return new_pml4;
}

void vmm_destroy_address_space(page_table_t *pml4) {
    if (!pml4 || pml4 == kernel_pml4) return;
    pmm_free_page(pml4);
}
