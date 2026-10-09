#include "apic_timer.h"
#include "apic.h"
#include "serial.h"
#include "task.h"

static volatile uint64_t timer_ticks = 0;

uint64_t apic_timer_get_ticks(void) {
    return timer_ticks;
}

void apic_timer_init(uint32_t frequency_hz) {
    serial_print("[*] Configuration de l APIC Timer pour le Scheduler...\n");

    /* 1. Mettre le TPR a 0 pour autoriser toutes les interruptions */
    lapic_write(0x080, 0x00);

    /* 2. Diviseur APIC : Divide by 16 (valeur 0x03) */
    lapic_write(0x3E0, 0x03);

    /* 3. LVT Timer : Mode Periodique (bit 17 = 1), Unmasked (bit 16 = 0), Vecteur 0x20 */
    lapic_write(0x320, 0x00020020);

    /* 4. Initial Count Register (0x380) */
    uint32_t count = 10000000 / frequency_hz;
    lapic_write(0x380, count);

    serial_print("[+] APIC Timer : Arme avec succes (Mode Periodique, Vecteur 0x20)!\n");
}
