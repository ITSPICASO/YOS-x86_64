#ifndef APIC_H
#define APIC_H

#include <stdint.h>

#define IA32_APIC_BASE_MSR        0x1B
#define IA32_APIC_BASE_MSR_ENABLE 0x800

#define LAPIC_PHYS_BASE           0xFEE00000ULL
#define LAPIC_VIRT_BASE           0xFFFFFFFFFE000000ULL

/* Registres LAPIC (offsets relatifs) */
#define LAPIC_ID                  0x0020
#define LAPIC_VERSION             0x0030
#define LAPIC_TPR                 0x0080 /* Task Priority Register */
#define LAPIC_EOI                 0x00B0 /* End of Interrupt */
#define LAPIC_SVR                 0x00F0 /* Spurious Vector Register */
#define LAPIC_ESR                 0x0280 /* Error Status Register */
#define LAPIC_TIMER               0x0320 /* LVT Timer Register */
#define LAPIC_TIMER_INITCNT       0x0380 /* Initial Count Register */
#define LAPIC_TIMER_CURRCNT       0x0390 /* Current Count Register */
#define LAPIC_TIMER_DIV           0x03E0 /* Divide Configuration Register */

#define LAPIC_ENABLE_BIT          (1 << 8)
#define APIC_SPURIOUS_IRQ         0xFF

void lapic_init(void);
void lapic_write(uint32_t reg, uint32_t val);
uint32_t lapic_read(uint32_t reg);
void lapic_eoi(void);

#endif
