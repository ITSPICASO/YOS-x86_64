#include "apic.h"
#include "serial.h"

#define IA32_APIC_BASE_MSR 0x1B
#define IA32_APIC_BASE_MSR_ENABLE 0x800

#define LAPIC_REG_ID        0x0020
#define LAPIC_REG_VERSION   0x0030
#define LAPIC_REG_TPR       0x0080
#define LAPIC_REG_EOI       0x00B0
#define LAPIC_REG_SVR       0x00F0
#define LAPIC_REG_TIMER     0x0320
#define LAPIC_REG_TICR      0x0380
#define LAPIC_REG_TCCR      0x0390
#define LAPIC_REG_TDCR      0x03E0

static uint64_t lapic_base = 0xFEE00000;

uint32_t lapic_read(uint32_t reg) {
    return *((volatile uint32_t *)(lapic_base + reg));
}

void lapic_write(uint32_t reg, uint32_t val) {
    *((volatile uint32_t *)(lapic_base + reg)) = val;
}

static inline void cpu_get_msr(uint32_t msr, uint32_t *lo, uint32_t *hi) {
    __asm__ volatile("rdmsr" : "=a"(*lo), "=d"(*hi) : "c"(msr));
}

static inline void cpu_set_msr(uint32_t msr, uint32_t lo, uint32_t hi) {
    __asm__ volatile("wrmsr" : : "a"(lo), "d"(hi), "c"(msr));
}

void lapic_init(void) {
    serial_print("[*] LAPIC: Debut initialisation...\n");

    /* 1. Activer le LAPIC au niveau MSR global */
    uint32_t lo, hi;
    cpu_get_msr(IA32_APIC_BASE_MSR, &lo, &hi);
    lo |= IA32_APIC_BASE_MSR_ENABLE;
    cpu_set_msr(IA32_APIC_BASE_MSR, lo, hi);

    /* 2. Base physique */
    lapic_base = (lo & 0xFFFFF000ULL);

    /* 3. Task Priority Register (TPR) -> 0 */
    lapic_write(LAPIC_REG_TPR, 0);

    /* 4. Spurious Interrupt Vector Register (SVR) -> bit 8 enable + vector 0xFF */
    lapic_write(LAPIC_REG_SVR, 0x1FF);

    uint32_t id = lapic_read(LAPIC_REG_ID) >> 24;
    uint32_t ver = lapic_read(LAPIC_REG_VERSION);

    serial_print("[+] Local APIC (LAPIC) Initialise via MMIO a 0x");
    serial_print_hex(lapic_base);
    serial_print("!\n");
    serial_print("  -> CPU LAPIC ID : ");
    serial_print_dec(id);
    serial_print("\n  -> Version      : 0x");
    serial_print_hex(ver);
    serial_print("\n");
}

void lapic_eoi(void) {
    lapic_write(LAPIC_REG_EOI, 0);
}
