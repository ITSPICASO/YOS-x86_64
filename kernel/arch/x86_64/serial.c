#include "serial.h"
#include "io.h"

int serial_init(void) {
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
    return 0;
}

static int serial_is_transmit_empty(void) {
    return inb(COM1 + 5) & 0x20;
}

void serial_putc(char c) {
    int timeout = 100000;
    while (serial_is_transmit_empty() == 0 && timeout > 0) {
        timeout--;
    }
    outb(COM1, (uint8_t)c);
}

void serial_print(const char *str) {
    if (!str) return;
    for (uint64_t i = 0; str[i] != 0; i++) {
        if (str[i] == 92 && str[i + 1] == 110) {
            serial_putc(13);
            serial_putc(10);
            i++;
            continue;
        }
        if (str[i] == 10) {
            serial_putc(13);
            serial_putc(10);
            continue;
        }
        serial_putc(str[i]);
    }
}

void serial_print_hex(uint64_t val) {
    const char *hex_digits = "0123456789ABCDEF";
    serial_print("0x");
    for (int i = 60; i >= 0; i -= 4) {
        uint8_t nibble = (val >> i) & 0xF;
        serial_putc(hex_digits[nibble]);
    }
}

void serial_print_dec(uint64_t val) {
    if (val == 0) {
        serial_putc(48);
        return;
    }
    char buffer[21];
    int i = 0;
    while (val > 0) {
        buffer[i++] = 48 + (val % 10);
        val /= 10;
    }
    while (i > 0) {
        serial_putc(buffer[--i]);
    }
}
