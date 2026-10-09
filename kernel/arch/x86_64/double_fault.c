#include "double_fault.h"
#include "idt.h"
#include "isr.h"

extern void isr8(void);

void double_fault_handler(interrupt_frame_t *frame) {
    (void)frame;
    /* Kernel Panic : Arret immediat du CPU */
    for (;;) {
        __asm__ volatile("cli; hlt");
    }
}

void double_fault_init(void) {
    uint8_t flags = 0x8E; /* Present, DPL 0, Interrupt Gate 64-bit */
    uint16_t cs = 0x08;
    uint8_t ist = 1;      /* Utilise IST1 du TSS */

    idt_set_gate(8, (uint64_t)isr8, cs, flags, ist);
    isr_register_handler(8, double_fault_handler);
}
