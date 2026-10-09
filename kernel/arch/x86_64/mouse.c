#include "wm.h"
#include "fb.h"
#include "mouse.h"
#include "ps2.h"
#include "io.h"
#include "serial.h"
#include "apic.h"
#include "ioapic.h"
#include "idt.h"

extern void mouse_stub(void);

static uint8_t mouse_cycle = 0;
static uint8_t mouse_bytes[3];
static mouse_state_t mouse_state = {0, 0, false, false, false};

static inline void mouse_wait_write(void) {
    while (inb(PS2_STATUS_PORT) & PS2_STATUS_INPUT_BUFFER_FULL) {
        __asm__ volatile("pause");
    }
}

static inline void mouse_wait_read(void) {
    while (!(inb(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT_BUFFER_FULL)) {
        __asm__ volatile("pause");
    }
}

static void mouse_write(uint8_t write_val) {
    mouse_wait_write();
    outb(PS2_COMMAND_PORT, 0xD4); /* Renvoyer le prochain octet au port 2 (Souris) */
    mouse_wait_write();
    outb(PS2_DATA_PORT, write_val);
}

static uint8_t mouse_read(void) {
    mouse_wait_read();
    return inb(PS2_DATA_PORT);
}

void mouse_handler(void) {
    uint8_t status = inb(PS2_STATUS_PORT);
    if (!(status & PS2_STATUS_OUTPUT_BUFFER_FULL)) {
        lapic_eoi();
        return;
    }

    uint8_t b = inb(PS2_DATA_PORT);

    /* Ne traiter que si la donnée provient du Port 2 (bit 5 = 1) */
    if (!(status & (1 << 5))) {
        lapic_eoi();
        return;
    }

    switch (mouse_cycle) {
        case 0:
            /* Le bit 3 du 1er octet doit toujours valoir 1 pour être synchronisé */
            if (b & 0x08) {
                mouse_bytes[0] = b;
                mouse_cycle = 1;
            }
            break;
        case 1:
            mouse_bytes[1] = b;
            mouse_cycle = 2;
            break;
        case 2:
            mouse_bytes[2] = b;
            mouse_cycle = 0;

            /* Décodage des boutons */
            mouse_state.btn_left   = (mouse_bytes[0] & 0x01) != 0;
            mouse_state.btn_right  = (mouse_bytes[0] & 0x02) != 0;
            mouse_state.btn_middle = (mouse_bytes[0] & 0x04) != 0;

            /* Déplacement relatif en X et Y (gestion du signe via bits 4 et 5) */
            int32_t dx = (int32_t)mouse_bytes[1];
            if (mouse_bytes[0] & 0x10) dx |= 0xFFFFFF00;

            int32_t dy = (int32_t)mouse_bytes[2];
            if (mouse_bytes[0] & 0x20) dy |= 0xFFFFFF00;

            mouse_state.x += dx;
            mouse_state.y -= dy; /* Inversion de l'axe Y pour repère écran */

            if (mouse_state.btn_left)  serial_print("[MOUSE] CLIC G\n");
            if (mouse_state.btn_right) serial_print("[MOUSE] CLIC D\n");
            wm_handle_mouse(mouse_state.x, mouse_state.y, mouse_state.btn_left);
            mouse_update();
            break;
    }

    lapic_eoi();
}

void mouse_init(void) {
    /* 1. Activer le Port 2 sur le contrôleur i8042 */
    mouse_wait_write();
    outb(PS2_COMMAND_PORT, PS2_CMD_ENABLE_PORT2);

    /* 2. Activer les interruptions du Port 2 dans l octet de config */
    mouse_wait_write();
    outb(PS2_COMMAND_PORT, PS2_CMD_READ_CONFIG_BYTE);
    mouse_wait_read();
    uint8_t config = inb(PS2_DATA_PORT);
    config |= (1 << 1); /* Interruption Port 2 activée */
    config &= ~(1 << 5); /* Port 2 clock enable */
    mouse_wait_write();
    outb(PS2_COMMAND_PORT, PS2_CMD_WRITE_CONFIG_BYTE);
    mouse_wait_write();
    outb(PS2_DATA_PORT, config);

    /* 3. Initialiser la souris : Mode par défaut */
    mouse_write(0xF6);
    mouse_read(); /* ACK (0xFA) */

    /* 4. Activer le flux de paquets de la souris */
    mouse_write(0xF4);
    mouse_read(); /* ACK (0xFA) */

    /* 5. Installer le handler dans l'IDT et router IRQ 12 via l'I/O APIC */
    idt_set_gate(MOUSE_IRQ_VECTOR, (uint64_t)mouse_stub, 0x08, 0x8E, 0);
    ioapic_set_irq(12, MOUSE_IRQ_VECTOR, 0);
    ioapic_unmask_irq(12);

    serial_print("[+] Souris PS/2 : Routee sur IRQ 12 -> Vecteur 0x2C (Activee).\n");
}

mouse_state_t mouse_get_state(void) {
    return mouse_state;
}

/* Bitmap classique du curseur (12x19) */
static const char cursor_bitmap[CURSOR_HEIGHT][CURSOR_WIDTH + 1] = {
    "X           ",
    "XX          ",
    "X.X         ",
    "X..X        ",
    "X...X       ",
    "X....X      ",
    "X.....X     ",
    "X......X    ",
    "X.......X   ",
    "X........X  ",
    "X.....XXXXX ",
    "X..X..X     ",
    "X.X X..X    ",
    "XX   X..X   ",
    "X     X..X  ",
    "      X..X  ",
    "       XX   ",
    "            ",
    "            "
};

static uint32_t cursor_saved_bg[CURSOR_HEIGHT][CURSOR_WIDTH];
static int32_t last_cursor_x = 0;
static int32_t last_cursor_y = 0;
static bool cursor_visible = false;

void mouse_init_cursor(void) {
    framebuffer_t *fb = fb_get_info();
    if (!fb || !fb->back_buffer) return;

    /* Placer la souris au centre de l'ecran au depart */
    mouse_state.x = fb->width / 2;
    mouse_state.y = fb->height / 2;
    last_cursor_x = mouse_state.x;
    last_cursor_y = mouse_state.y;

    /* Sauvegarder l'arriere-plan initial */
    for (int y = 0; y < CURSOR_HEIGHT; y++) {
        for (int x = 0; x < CURSOR_WIDTH; x++) {
            uint32_t px = mouse_state.x + x;
            uint32_t py = mouse_state.y + y;
            if (px < fb->width && py < fb->height) {
                cursor_saved_bg[y][x] = fb->back_buffer[py * (fb->pitch / 4) + px];
            }
        }
    }

    /* Dessiner le curseur */
    for (int y = 0; y < CURSOR_HEIGHT; y++) {
        for (int x = 0; x < CURSOR_WIDTH; x++) {
            char pixel = cursor_bitmap[y][x];
            if (pixel == 'X') {
                fb_putpixel(mouse_state.x + x, mouse_state.y + y, 0x00000000);
            } else if (pixel == '.') {
                fb_putpixel(mouse_state.x + x, mouse_state.y + y, 0x00FFFFFF);
            }
        }
    }

    cursor_visible = true;
    fb_flip();
}

void mouse_update(void) {
    framebuffer_t *fb = fb_get_info();
    if (!fb || !fb->back_buffer || !cursor_visible) return;

    /* Verifier si la position a change */
    if (mouse_state.x == last_cursor_x && mouse_state.y == last_cursor_y) return;

    /* 1. Clamper les coordonnees aux limites de l'ecran */
    if (mouse_state.x < 0) mouse_state.x = 0;
    if (mouse_state.y < 0) mouse_state.y = 0;
    if (mouse_state.x >= (int32_t)(fb->width - CURSOR_WIDTH)) mouse_state.x = fb->width - CURSOR_WIDTH - 1;
    if (mouse_state.y >= (int32_t)(fb->height - CURSOR_HEIGHT)) mouse_state.y = fb->height - CURSOR_HEIGHT - 1;

    /* 2. Restaurer l'arriere-plan a l'ancienne position */
    for (int y = 0; y < CURSOR_HEIGHT; y++) {
        for (int x = 0; x < CURSOR_WIDTH; x++) {
            fb_putpixel(last_cursor_x + x, last_cursor_y + y, cursor_saved_bg[y][x]);
        }
    }

    /* 3. Sauvegarder l'arriere-plan a la nouvelle position */
    for (int y = 0; y < CURSOR_HEIGHT; y++) {
        for (int x = 0; x < CURSOR_WIDTH; x++) {
            uint32_t px = mouse_state.x + x;
            uint32_t py = mouse_state.y + y;
            if (px < fb->width && py < fb->height) {
                cursor_saved_bg[y][x] = fb->back_buffer[py * (fb->pitch / 4) + px];
            }
        }
    }

    /* 4. Dessiner le curseur a la nouvelle position */
    for (int y = 0; y < CURSOR_HEIGHT; y++) {
        for (int x = 0; x < CURSOR_WIDTH; x++) {
            char pixel = cursor_bitmap[y][x];
            if (pixel == 'X') {
                fb_putpixel(mouse_state.x + x, mouse_state.y + y, 0x00000000);
            } else if (pixel == '.') {
                fb_putpixel(mouse_state.x + x, mouse_state.y + y, 0x00FFFFFF);
            }
        }
    }

    /* 5. Transferer UNIQUEMENT les zones modifiees (Dirty Rectangles) */
    fb_flip_rect(last_cursor_x, last_cursor_y, CURSOR_WIDTH, CURSOR_HEIGHT);
    fb_flip_rect(mouse_state.x, mouse_state.y, CURSOR_WIDTH, CURSOR_HEIGHT);

    last_cursor_x = mouse_state.x;
    last_cursor_y = mouse_state.y;
}
