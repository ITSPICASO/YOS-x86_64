#ifndef APIC_TIMER_H
#define APIC_TIMER_H

#include <stdint.h>
#include "isr.h"

#define APIC_TIMER_IRQ 32

void apic_timer_init(uint32_t frequency_hz);
uint64_t apic_timer_get_ticks(void);

#endif
