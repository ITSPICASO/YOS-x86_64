#include "serial.h"
#include "isr.h"
#include "idt.h"

extern void isr0(void);
extern void isr1(void);
extern void isr2(void);
extern void isr3(void);
extern void isr4(void);
extern void isr5(void);
extern void isr6(void);
extern void isr7(void);
extern void isr8(void);
extern void isr9(void);
extern void isr10(void);
extern void isr11(void);
extern void isr12(void);
extern void isr13(void);
extern void isr14(void);
extern void isr15(void);
extern void isr16(void);
extern void isr17(void);
extern void isr18(void);
extern void isr19(void);
extern void isr20(void);
extern void isr21(void);
extern void isr22(void);
extern void isr23(void);
extern void isr24(void);
extern void isr25(void);
extern void isr26(void);
extern void isr27(void);
extern void isr28(void);
extern void isr29(void);
extern void isr30(void);
extern void isr31(void);

static isr_handler_t isr_handlers[256] = {0};

void isr_register_handler(uint8_t n, isr_handler_t handler) {
    isr_handlers[n] = handler;
}

void isr_install(void) {
    uint8_t flags = 0x8E;
    uint16_t cs = 0x08;
    uint8_t ist = 0;

    idt_set_gate(0,  (uint64_t)isr0,  cs, flags, ist);
    idt_set_gate(1,  (uint64_t)isr1,  cs, flags, ist);
    idt_set_gate(2,  (uint64_t)isr2,  cs, flags, ist);
    idt_set_gate(3,  (uint64_t)isr3,  cs, flags, ist);
    idt_set_gate(4,  (uint64_t)isr4,  cs, flags, ist);
    idt_set_gate(5,  (uint64_t)isr5,  cs, flags, ist);
    idt_set_gate(6,  (uint64_t)isr6,  cs, flags, ist);
    idt_set_gate(7,  (uint64_t)isr7,  cs, flags, ist);
    idt_set_gate(8,  (uint64_t)isr8,  cs, flags, ist);
    idt_set_gate(9,  (uint64_t)isr9,  cs, flags, ist);
    idt_set_gate(10, (uint64_t)isr10, cs, flags, ist);
    idt_set_gate(11, (uint64_t)isr11, cs, flags, ist);
    idt_set_gate(12, (uint64_t)isr12, cs, flags, ist);
    idt_set_gate(13, (uint64_t)isr13, cs, flags, ist);
    idt_set_gate(14, (uint64_t)isr14, cs, flags, ist);
    idt_set_gate(15, (uint64_t)isr15, cs, flags, ist);
    idt_set_gate(16, (uint64_t)isr16, cs, flags, ist);
    idt_set_gate(17, (uint64_t)isr17, cs, flags, ist);
    idt_set_gate(18, (uint64_t)isr18, cs, flags, ist);
    idt_set_gate(19, (uint64_t)isr19, cs, flags, ist);
    idt_set_gate(20, (uint64_t)isr20, cs, flags, ist);
    idt_set_gate(21, (uint64_t)isr21, cs, flags, ist);
    idt_set_gate(22, (uint64_t)isr22, cs, flags, ist);
    idt_set_gate(23, (uint64_t)isr23, cs, flags, ist);
    idt_set_gate(24, (uint64_t)isr24, cs, flags, ist);
    idt_set_gate(25, (uint64_t)isr25, cs, flags, ist);
    idt_set_gate(26, (uint64_t)isr26, cs, flags, ist);
    idt_set_gate(27, (uint64_t)isr27, cs, flags, ist);
    idt_set_gate(28, (uint64_t)isr28, cs, flags, ist);
    idt_set_gate(29, (uint64_t)isr29, cs, flags, ist);
    idt_set_gate(30, (uint64_t)isr30, cs, flags, ist);
    idt_set_gate(31, (uint64_t)isr31, cs, flags, ist);
}

void isr_handler_dispatch(interrupt_frame_t *frame) {
    if (isr_handlers[frame->int_no] != 0) {
        isr_handlers[frame->int_no](frame);
        return;
    }

    if (frame->int_no < 32) {
        serial_print("[-] EXCEPTION CPU DETECTEE : Vector ");
        serial_print_dec(frame->int_no);
        serial_print(" | Error Code: 0x");
        serial_print_hex(frame->err_code);
        serial_print(" | RIP: 0x");
        serial_print_hex(frame->rip);
        serial_print("\n");
        for (;;) {
            __asm__ volatile("cli; hlt");
        }
    }
}
