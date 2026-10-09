#include "ioapic.h"
#include "serial.h"

#define IOREGSEL 0x00
#define IOWIN    0x10

#define IOAPIC_REG_ID      0x00
#define IOAPIC_REG_VERSION 0x01
#define IOAPIC_REG_REDTBL  0x10

static uint64_t ioapic_base = 0xFEC00000;

void ioapic_write(uint8_t reg, uint32_t val) {
    volatile uint32_t *regsel = (volatile uint32_t *)(ioapic_base + IOREGSEL);
    volatile uint32_t *data   = (volatile uint32_t *)(ioapic_base + IOWIN);
    *regsel = reg;
    *data   = val;
}

uint32_t ioapic_read(uint8_t reg) {
    volatile uint32_t *regsel = (volatile uint32_t *)(ioapic_base + IOREGSEL);
    volatile uint32_t *data   = (volatile uint32_t *)(ioapic_base + IOWIN);
    *regsel = reg;
    return *data;
}

void ioapic_init(uint64_t base) {
    serial_print("[*] IOAPIC: Debut initialisation...\n");
    if (base != 0) {
        ioapic_base = base;
    }

    uint32_t ver_reg = ioapic_read(IOAPIC_REG_VERSION);
    uint32_t version = ver_reg & 0xFF;
    uint32_t max_entries = ((ver_reg >> 16) & 0xFF) + 1;

    serial_print("[+] I/O APIC Initialise via MMIO a 0x");
    serial_print_hex(ioapic_base);
    serial_print("\n  -> Version        : 0x");
    serial_print_hex(version);
    serial_print("\n  -> Entrees Max    : ");
    serial_print_dec(max_entries);
    serial_print("\n");

    /* Masquer toutes les interruptions par defaut */
    for (uint32_t i = 0; i < max_entries; i++) {
        uint8_t low_index  = IOAPIC_REG_REDTBL + (i * 2);
        uint8_t high_index = low_index + 1;

        ioapic_write(high_index, 0);
        ioapic_write(low_index, 0x00010000); /* Bit 16 = Masked */
    }

    serial_print("[+] I/O APIC: Toutes les interruptions sont masquees.\n");
}

void ioapic_set_irq(uint8_t irq, uint8_t vector, uint32_t target_cpu) {
    uint8_t low_index  = IOAPIC_REG_REDTBL + (irq * 2);
    uint8_t high_index = low_index + 1;

    uint32_t low = vector;
    uint32_t high = target_cpu << 24;

    ioapic_write(high_index, high);
    ioapic_write(low_index, low);
}

void ioapic_unmask_irq(uint8_t irq) {
    uint8_t low_index = IOAPIC_REG_REDTBL + (irq * 2);
    uint32_t val = ioapic_read(low_index);
    val &= ~(1 << 16);
    ioapic_write(low_index, val);
}

void ioapic_mask_irq(uint8_t irq) {
    uint8_t low_index = IOAPIC_REG_REDTBL + (irq * 2);
    uint32_t val = ioapic_read(low_index);
    val |= (1 << 16);
    ioapic_write(low_index, val);
}
