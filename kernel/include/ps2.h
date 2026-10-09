#ifndef PS2_H
#define PS2_H

#include <stdint.h>
#include <stdbool.h>

#define PS2_DATA_PORT    0x60
#define PS2_STATUS_PORT  0x64
#define PS2_COMMAND_PORT 0x64

/* Bits du registre de statut */
#define PS2_STATUS_OUTPUT_BUFFER_FULL (1 << 0)
#define PS2_STATUS_INPUT_BUFFER_FULL  (1 << 1)

/* Commandes du contrôleur */
#define PS2_CMD_READ_CONFIG_BYTE      0x20
#define PS2_CMD_WRITE_CONFIG_BYTE     0x60
#define PS2_CMD_DISABLE_PORT2         0xA7
#define PS2_CMD_ENABLE_PORT2          0xA8
#define PS2_CMD_TEST_PORT2            0xA9
#define PS2_CMD_TEST_CONTROLLER       0xAA
#define PS2_CMD_TEST_PORT1            0xAB
#define PS2_CMD_DISABLE_PORT1         0xAD
#define PS2_CMD_ENABLE_PORT1          0xAE

void ps2_init(void);

#endif
