#ifndef GDT_H
#define GDT_H

#include <stdint.h>

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

// Entrée TSS 64-bit (16 octets au total)
struct gdt_tss_entry {
    struct gdt_entry low;
    uint32_t base_highest;
    uint32_t reserved;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

void gdt_init(void);
void gdt_set_tss(uint64_t tss_base, uint32_t tss_limit);

#endif
