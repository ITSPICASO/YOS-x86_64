#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>

#define COM1 0x3F8

int serial_init(void);
void serial_putc(char c);
void serial_print(const char *str);
void serial_print_hex(uint64_t val);
void serial_print_dec(uint64_t val);

#endif
