#include "idt.h"
#include "serial.h"

static struct idt_entry idt[256];
static struct idt_ptr   idtp;

extern void idt_flush(uint64_t);

/* ISRs externes */
extern void isr0(void);
extern void isr8(void);
extern void isr13(void);
extern void isr14(void);
extern void keyboard_stub(void);
extern void mouse_stub(void);
extern void apic_timer_stub(void);

void idt_set_gate(uint8_t num, uint64_t base, uint16_t sel, uint8_t flags, uint8_t ist) {
    idt[num].offset_low  = (uint16_t)(base & 0xFFFF);
    idt[num].offset_mid  = (uint16_t)((base >> 16) & 0xFFFF);
    idt[num].offset_high = (uint32_t)((base >> 32) & 0xFFFFFFFF);
    idt[num].selector    = sel;
    idt[num].ist         = ist;
    idt[num].type_attr   = flags;
    idt[num].zero        = 0;
}

void idt_init(void) {
    idtp.limit = (sizeof(struct idt_entry) * 256) - 1;
    idtp.base  = (uint64_t)&idt;

    for (int i = 0; i < 256; i++) {
        idt_set_gate(i, 0, 0, 0, 0);
    }

    /* Exceptions de base */
    idt_set_gate(0,  (uint64_t)isr0,          0x08, 0x8E, 0);
    idt_set_gate(8,  (uint64_t)isr8,          0x08, 0x8E, 0);
    idt_set_gate(13, (uint64_t)isr13,         0x08, 0x8E, 0);
    idt_set_gate(14, (uint64_t)isr14,         0x08, 0x8E, 0);

    /* Hardware Interrupts (APIC Timer, Keyboard, Mouse) */
    idt_set_gate(0x20, (uint64_t)apic_timer_stub, 0x08, 0x8E, 0);
    idt_set_gate(0x21, (uint64_t)keyboard_stub,   0x08, 0x8E, 0);
    idt_set_gate(0x2C, (uint64_t)mouse_stub,      0x08, 0x8E, 0);

    idt_flush((uint64_t)&idtp);
    serial_print("[+] IDT : Chargee a l adresse 0x");
    serial_print_hex((uint64_t)&idt);
    serial_print("\n");
}
