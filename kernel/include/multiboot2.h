#ifndef MULTIBOOT2_H
#define MULTIBOOT2_H

#include <stdint.h>
#include <stddef.h>

#define MULTIBOOT_TAG_TYPE_END       0
#define MULTIBOOT_TAG_TYPE_MODULE    3
#define MULTIBOOT_TAG_TYPE_MMAP      6
#define MULTIBOOT_TAG_TYPE_FRAMEBUFFER 8

struct multiboot_tag_framebuffer_common {
    uint32_t type;
    uint32_t size;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t  framebuffer_bpp;
    uint8_t  framebuffer_type;
    uint16_t reserved;
} __attribute__((packed));

typedef struct {
    uint64_t addr;
    uint32_t pitch;
    uint32_t width;
    uint32_t height;
    uint8_t  bpp;
} fb_info_t;

extern fb_info_t fb_info;
fb_info_t *multiboot_get_fb(void);


#define MULTIBOOT_MEMORY_AVAILABLE        1
#define MULTIBOOT_MEMORY_RESERVED         2
#define MULTIBOOT_MEMORY_ACPI_RECLAIMABLE 3
#define MULTIBOOT_MEMORY_NVS              4
#define MULTIBOOT_MEMORY_BADRAM           5

struct multiboot_tag {
    uint32_t type;
    uint32_t size;
};

struct multiboot_mmap_entry {
    uint64_t addr;
    uint64_t len;
    uint32_t type;
    uint32_t zero;
} __attribute__((packed));

struct multiboot_tag_mmap {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
    struct multiboot_mmap_entry entries[];
} __attribute__((packed));

struct multiboot_tag_module {
    uint32_t type;
    uint32_t size;
    uint32_t mod_start;
    uint32_t mod_end;
    char     cmdline[];
} __attribute__((packed));

void multiboot_parse_mmap(uint64_t addr);
uint64_t multiboot_get_module_start(void);
uint64_t multiboot_get_module_size(void);

#endif

extern uint64_t module_start_addr;
extern uint64_t module_total_size;
