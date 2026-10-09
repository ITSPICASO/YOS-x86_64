#include "tss.h"
#include "gdt.h"
#include <stddef.h>

extern void tss_flush(void);

static tss_entry_t tss __attribute__((aligned(16)));
static uint8_t kernel_stack[16384] __attribute__((aligned(16)));
static uint8_t df_stack[16384] __attribute__((aligned(16)));

void tss_init(void) {
    uint8_t *ptr = (uint8_t *)&tss;
    for (size_t i = 0; i < sizeof(tss); i++) ptr[i] = 0;

    tss.rsp0 = (uint64_t)kernel_stack + sizeof(kernel_stack);
    tss.ist1 = (uint64_t)df_stack + sizeof(df_stack);
    tss.iomap_base = (uint16_t)sizeof(tss);

    gdt_set_tss((uint64_t)&tss, sizeof(tss) - 1);
    tss_flush();
}

void tss_set_rsp0(uint64_t rsp0) {
    tss.rsp0 = rsp0;
}
