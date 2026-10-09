#include "ps2.h"
#include "io.h"
#include "serial.h"

static inline void ps2_wait_write(void) {
    while (inb(PS2_STATUS_PORT) & PS2_STATUS_INPUT_BUFFER_FULL) {
        __asm__ volatile("pause");
    }
}

static inline void ps2_wait_read(void) {
    while (!(inb(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT_BUFFER_FULL)) {
        __asm__ volatile("pause");
    }
}

void ps2_init(void) {
    serial_print("[*] Initialisation du controleur i8042 (PS/2)...\\n");

    /* 1. Desactiver les ports 1 et 2 */
    ps2_wait_write();
    outb(PS2_COMMAND_PORT, PS2_CMD_DISABLE_PORT1);
    ps2_wait_write();
    outb(PS2_COMMAND_PORT, PS2_CMD_DISABLE_PORT2);

    /* 2. Vider le buffer residuel */
    while (inb(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT_BUFFER_FULL) {
        inb(PS2_DATA_PORT);
    }

    /* 3. Lire l octet de configuration */
    ps2_wait_write();
    outb(PS2_COMMAND_PORT, PS2_CMD_READ_CONFIG_BYTE);
    ps2_wait_read();
    uint8_t config = inb(PS2_DATA_PORT);

    /* Activer interruptions Port 1 + Translation Scancode Set 1 */
    config |= (1 << 0);
    config &= ~(1 << 1);
    config |= (1 << 6);

    /* 4. Ecrire l octet de configuration */
    ps2_wait_write();
    outb(PS2_COMMAND_PORT, PS2_CMD_WRITE_CONFIG_BYTE);
    ps2_wait_write();
    outb(PS2_DATA_PORT, config);

    /* 5. Activer le Port 1 (Clavier) */
    ps2_wait_write();
    outb(PS2_COMMAND_PORT, PS2_CMD_ENABLE_PORT1);

    serial_print("[+] Controleur PS/2 configure et Port 1 active.\\n");
}
