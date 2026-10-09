#include "rtc.h"
#include "io.h"

#define CMOS_ADDRESS 0x70
#define CMOS_DATA    0x71

static int get_update_in_progress_flag(void) {
    outb(CMOS_ADDRESS, 0x0A);
    return (inb(CMOS_DATA) & 0x80);
}

static uint8_t get_rtc_register(int reg) {
    outb(CMOS_ADDRESS, reg);
    return inb(CMOS_DATA);
}

static uint8_t bcd_to_bin(uint8_t val) {
    return ((val >> 4) * 10) + (val & 0x0F);
}

void rtc_read_time(rtc_time_t *time) {
    while (get_update_in_progress_flag());

    uint8_t sec  = get_rtc_register(0x00);
    uint8_t min  = get_rtc_register(0x02);
    uint8_t hour = get_rtc_register(0x04);
    uint8_t day  = get_rtc_register(0x07);
    uint8_t mon  = get_rtc_register(0x08);
    uint8_t yr   = get_rtc_register(0x09);

    uint8_t registerB = get_rtc_register(0x0B);

    /* Ila kan BCD, n7ewlouh l binary */
    if (!(registerB & 0x04)) {
        sec  = bcd_to_bin(sec);
        min  = bcd_to_bin(min);
        hour = ((hour & 0x7F) != 0) ? bcd_to_bin(hour & 0x7F) : bcd_to_bin(hour);
        day  = bcd_to_bin(day);
        mon  = bcd_to_bin(mon);
        yr   = bcd_to_bin(yr);
    }

    /* Format 12h -> 24h */
    if (!(registerB & 0x02) && (hour & 0x80)) {
        hour = ((hour & 0x7F) + 12) % 24;
    }

    time->second = sec;
    time->minute = min;
    time->hour   = hour;
    time->day    = day;
    time->month  = mon;
    time->year   = 2000 + yr;
}
