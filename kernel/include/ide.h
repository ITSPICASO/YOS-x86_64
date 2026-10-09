#ifndef IDE_H
#define IDE_H

#include <stdint.h>
#include <stdbool.h>

#define ATA_PRIMARY_IO      0x1F0
#define ATA_PRIMARY_CTRL    0x3F6

#define ATA_REG_DATA        0x00
#define ATA_REG_ERROR       0x01
#define ATA_REG_SECCOUNT    0x02
#define ATA_REG_LBA_LO      0x03
#define ATA_REG_LBA_MID     0x04
#define ATA_REG_LBA_HI      0x05
#define ATA_REG_DRIVE_SEL   0x06
#define ATA_REG_STATUS      0x07
#define ATA_REG_COMMAND     0x07

/* Status register bits */
#define ATA_STATUS_ERR      (1 << 0)
#define ATA_STATUS_DRQ      (1 << 3)
#define ATA_STATUS_SRV      (1 << 4)
#define ATA_STATUS_DF       (1 << 5)
#define ATA_STATUS_RDY      (1 << 6)
#define ATA_STATUS_BSY      (1 << 7)

/* Commandes */
#define ATA_CMD_READ_PIO    0x20
#define ATA_CMD_WRITE_PIO   0x30
#define ATA_CMD_IDENTIFY    0xEC

void ide_init(void);
bool ide_read_sectors(uint32_t lba, uint8_t sector_count, uint8_t *buffer);
bool ide_write_sectors(uint32_t lba, uint8_t sector_count, const uint8_t *buffer);

#endif
