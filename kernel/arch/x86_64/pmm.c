#include "pmm.h"
#include "serial.h"

extern uint64_t _kernel_end;

static uint64_t *pmm_bitmap = 0;
static uint64_t total_pages = 0;
static uint64_t used_pages = 0;
static uint64_t max_usable_address = 0;

static inline void bitmap_set(uint64_t page)
{
    pmm_bitmap[page / 64] |= (1ULL << (page % 64));
}

static inline void bitmap_clear(uint64_t page)
{
    pmm_bitmap[page / 64] &= ~(1ULL << (page % 64));
}

static inline int bitmap_test(uint64_t page)
{
    return (pmm_bitmap[page / 64] & (1ULL << (page % 64))) != 0;
}

void pmm_init(uint64_t multiboot_addr)
{
    uint32_t total_size = *(uint32_t *)multiboot_addr;
    uint64_t current_addr = multiboot_addr + 8;
    uint64_t end_addr = multiboot_addr + total_size;
    struct multiboot_tag_mmap *mmap_tag = 0;

    while (current_addr < end_addr) {
        struct multiboot_tag *tag = (struct multiboot_tag *)current_addr;
        if (tag->type == MULTIBOOT_TAG_TYPE_END) break;
        if (tag->type == MULTIBOOT_TAG_TYPE_MMAP) {
            mmap_tag = (struct multiboot_tag_mmap *)tag;
            break;
        }
        current_addr += ((tag->size + 7) & ~7);
    }

    if (!mmap_tag) return;

    uint64_t entries_count = (mmap_tag->size - sizeof(struct multiboot_tag_mmap)) / mmap_tag->entry_size;

    for (uint64_t i = 0; i < entries_count; i++) {
        struct multiboot_mmap_entry *entry = (struct multiboot_mmap_entry *)((uint8_t *)mmap_tag->entries + (i * mmap_tag->entry_size));
        if (entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
            uint64_t top = entry->addr + entry->len;
            if (top > max_usable_address) {
                max_usable_address = top;
            }
        }
    }

    total_pages = max_usable_address / PAGE_SIZE;
    uint64_t bitmap_size = (total_pages / 8) + 8;

    /* Placer le Bitmap apres le kernel physique (convertir adresse virtuelle higher-half vers physique) */
    uint64_t kernel_phys_end = ((uint64_t)&_kernel_end) - 0xFFFFFFFF80000000;
    pmm_bitmap = (uint64_t *)(kernel_phys_end + 0xFFFFFFFF80000000);

    /* Par defaut, on marque toute la memoire comme occupee */
    for (uint64_t i = 0; i < bitmap_size / 8; i++) {
        pmm_bitmap[i] = 0xFFFFFFFFFFFFFFFFULL;
    }
    used_pages = total_pages;

    /* Liberer uniquement les regions marquees disponibles */
    for (uint64_t i = 0; i < entries_count; i++) {
        struct multiboot_mmap_entry *entry = (struct multiboot_mmap_entry *)((uint8_t *)mmap_tag->entries + (i * mmap_tag->entry_size));
        if (entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
            uint64_t page_start = entry->addr / PAGE_SIZE;
            uint64_t page_count = entry->len / PAGE_SIZE;
            for (uint64_t p = 0; p < page_count; p++) {
                bitmap_clear(page_start + p);
                used_pages--;
            }
        }
    }

    /* Re-reserver le 1er Mo complet (BIOS, IVT, video) */
    for (uint64_t p = 0; p < 0x100000 / PAGE_SIZE; p++) {
        if (!bitmap_test(p)) {
            bitmap_set(p);
            used_pages++;
        }
    }

    /* Re-reserver le bitmap lui-meme */
    uint64_t bm_phys = kernel_phys_end;
    for (uint64_t p = bm_phys / PAGE_SIZE; p < (bm_phys + bitmap_size + PAGE_SIZE - 1) / PAGE_SIZE; p++) {
        if (!bitmap_test(p)) {
            bitmap_set(p);
            used_pages++;
        }
    }

    /* Re-reserver les modules Multiboot2 (Initrd TAR) */
    current_addr = multiboot_addr + 8;
    while (current_addr < end_addr) {
        struct multiboot_tag *tag = (struct multiboot_tag *)current_addr;
        if (tag->type == MULTIBOOT_TAG_TYPE_END) break;

        if (tag->type == MULTIBOOT_TAG_TYPE_MODULE) {
            struct multiboot_tag_module *mod = (struct multiboot_tag_module *)tag;
            uint64_t mod_start_page = mod->mod_start / PAGE_SIZE;
            uint64_t mod_end_page = (mod->mod_end + PAGE_SIZE - 1) / PAGE_SIZE;
            for (uint64_t p = mod_start_page; p < mod_end_page; p++) {
                if (!bitmap_test(p)) {
                    bitmap_set(p);
                    used_pages++;
                }
            }
        }
        current_addr += ((tag->size + 7) & ~7);
    }

    /* Re-reserver la memoire occupee par le Kernel et le Bitmap */
    uint64_t kernel_and_bitmap_pages = (kernel_phys_end + bitmap_size) / PAGE_SIZE + 1;
    for (uint64_t p = 0x100000 / PAGE_SIZE; p < kernel_and_bitmap_pages; p++) {
        if (!bitmap_test(p)) {
            bitmap_set(p);
            used_pages++;
        }
    }
}

void *pmm_alloc_page(void)
{
    for (uint64_t i = 0; i < total_pages; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            used_pages++;
            return (void *)(i * PAGE_SIZE);
        }
    }
    return 0;
}

void pmm_free_page(void *addr)
{
    uint64_t page = ((uint64_t)addr) / PAGE_SIZE;
    if (page < total_pages && bitmap_test(page)) {
        bitmap_clear(page);
        used_pages--;
    }
}

void *pmm_alloc_contiguous(size_t count)
{
    uint64_t consecutive = 0;
    uint64_t start_page = 0;

    for (uint64_t i = 0; i < total_pages; i++) {
        if (!bitmap_test(i)) {
            if (consecutive == 0) start_page = i;
            consecutive++;
            if (consecutive == count) {
                for (uint64_t p = 0; p < count; p++) {
                    bitmap_set(start_page + p);
                    used_pages++;
                }
                return (void *)(start_page * PAGE_SIZE);
            }
        } else {
            consecutive = 0;
        }
    }
    return 0;
}

uint64_t pmm_get_total_memory(void) { return total_pages * PAGE_SIZE; }
uint64_t pmm_get_used_memory(void) { return used_pages * PAGE_SIZE; }
uint64_t pmm_get_free_memory(void) { return (total_pages - used_pages) * PAGE_SIZE; }
