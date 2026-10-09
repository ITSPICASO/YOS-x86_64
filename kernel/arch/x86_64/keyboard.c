#include "wm.h"
#include "keyboard.h"
#include "ps2.h"
#include "io.h"
#include "serial.h"
#include "apic.h"
#include "ioapic.h"
#include "idt.h"

extern void keyboard_stub(void);

static bool shift_pressed = false;

static const char scancode_ascii_nomod[128] = {
    0,   27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,   '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0,   ' ', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

static const char scancode_ascii_shift[128] = {
    0,   27,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,   'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '\"', '~',
    0,   '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0,   ' ', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

#define KBD_BUFFER_SIZE 256
static char kbd_buffer[KBD_BUFFER_SIZE];
static volatile uint32_t kbd_head = 0;
static volatile uint32_t kbd_tail = 0;

bool keyboard_has_char(void) {
    return kbd_head != kbd_tail;
}

char keyboard_getchar(void) {
    if (kbd_head == kbd_tail) return 0;
    char c = kbd_buffer[kbd_tail];
    kbd_tail = (kbd_tail + 1) % KBD_BUFFER_SIZE;
    return c;
}

static void kbd_push_char(char c) {
    uint32_t next = (kbd_head + 1) % KBD_BUFFER_SIZE;
    if (next != kbd_tail) {
        kbd_buffer[kbd_head] = c;
        kbd_head = next;
    }
}

void keyboard_handler(void) {
    if (inb(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT_BUFFER_FULL) {
        uint8_t scancode = inb(PS2_DATA_PORT);

        if (scancode == 0x2A || scancode == 0x36) {
            shift_pressed = true;
        } else if (scancode == 0xAA || scancode == 0xB6) {
            shift_pressed = false;
        } else if (!(scancode & 0x80)) {
            char c = shift_pressed ? scancode_ascii_shift[scancode] : scancode_ascii_nomod[scancode];
            if (c != 0) {
                kbd_push_char(c);
                wm_handle_keyboard(c);
                serial_print("[KBD] Touche: ");
                serial_putc(c);
                serial_print("\n");
            }
        }
    }

    lapic_eoi();
}

void keyboard_init(void) {
    /* Vider les données résiduelles */
    while (inb(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT_BUFFER_FULL) {
        inb(PS2_DATA_PORT);
    }

    idt_set_gate(KEYBOARD_IRQ_VECTOR, (uint64_t)keyboard_stub, 0x08, 0x8E, 0);
    ioapic_set_irq(1, KEYBOARD_IRQ_VECTOR, 0);
    ioapic_unmask_irq(1);

    serial_print("[+] Clavier PS/2 : Route sur IRQ 1 -> Vecteur 0x21.\n");
}
