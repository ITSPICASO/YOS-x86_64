#include "gdt.h"
#include "tss.h"

#define GDT_ENTRIES 7

static struct gdt_entry gdt[GDT_ENTRIES] __attribute__((aligned(16)));
static struct gdt_ptr gp;

extern void gdt_flush(uint64_t);

static void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[num].limit_low   = (uint16_t)(limit & 0xFFFF);
    gdt[num].base_low    = (uint16_t)(base & 0xFFFF);
    gdt[num].base_middle = (uint8_t)((base >> 16) & 0xFF);
    gdt[num].access      = access;
    gdt[num].granularity = (uint8_t)(((limit >> 16) & 0x0F) | (gran & 0xF0));
    gdt[num].base_high   = (uint8_t)((base >> 24) & 0xFF);
}

void gdt_set_tss(uint64_t base, uint32_t limit) {
    struct gdt_tss_entry *tss_desc = (struct gdt_tss_entry *)&gdt[5];
    tss_desc->low.limit_low   = (uint16_t)(limit & 0xFFFF);
    tss_desc->low.base_low    = (uint16_t)(base & 0xFFFF);
    tss_desc->low.base_middle = (uint8_t)((base >> 16) & 0xFF);
    tss_desc->low.access      = 0x89;
    tss_desc->low.granularity = (uint8_t)((limit >> 16) & 0x0F);
    tss_desc->low.base_high   = (uint8_t)((base >> 24) & 0xFF);
    tss_desc->base_highest    = (uint32_t)(base >> 32);
    tss_desc->reserved        = 0;
}

void gdt_init(void) {
    gdt_set_gate(0, 0, 0, 0x00, 0x00);
    gdt_set_gate(1, 0, 0, 0x9A, 0x20);
    gdt_set_gate(2, 0, 0, 0x92, 0x00);
    gdt_set_gate(3, 0, 0, 0xF2, 0x00);
    gdt_set_gate(4, 0, 0, 0xFA, 0x20);
    gdt_set_gate(5, 0, 0, 0x00, 0x00);
    gdt_set_gate(6, 0, 0, 0x00, 0x00);

    gp.limit = (uint16_t)(sizeof(gdt) - 1);
    gp.base  = (uint64_t)&gdt;

    gdt_flush((uint64_t)&gp);
}
