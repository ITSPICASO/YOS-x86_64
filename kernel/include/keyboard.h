#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>
#include <stdbool.h>

#define KEYBOARD_IRQ_VECTOR 0x21

void keyboard_init(void);
void keyboard_handler(void);
char keyboard_getchar(void);
bool keyboard_has_char(void);

#endif
