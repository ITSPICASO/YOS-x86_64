#ifndef IOAPIC_H
#define IOAPIC_H

#include <stdint.h>

#define IOAPIC_REGSEL       0x00
#define IOAPIC_IOWIN        0x10

#define IOAPIC_ID           0x00
#define IOAPIC_VER          0x01
#define IOAPIC_ARB          0x02
#define IOAPIC_REDTBL(n)    (0x10 + 2 * (n))

void ioapic_init(uint64_t phys_base);
uint32_t ioapic_read(uint8_t reg);
void ioapic_write(uint8_t reg, uint32_t value);
void ioapic_set_irq(uint8_t irq, uint8_t vector, uint32_t target_cpu);
void ioapic_mask_irq(uint8_t irq);
void ioapic_unmask_irq(uint8_t irq);

#endif
