#include "ide.h"
#include "io.h"
#include "serial.h"

static inline void ata_wait_bsy(void) {
    while (inb(ATA_PRIMARY_IO + ATA_REG_STATUS) & ATA_STATUS_BSY) {
        __asm__ volatile("pause");
    }
}

static inline void ata_wait_drq(void) {
    while (!(inb(ATA_PRIMARY_IO + ATA_REG_STATUS) & ATA_STATUS_DRQ)) {
        __asm__ volatile("pause");
    }
}

void ide_init(void) {
    serial_print("[*] Initialisation du disque IDE ATA Primary Master...\n");

    /* 1. Sélectionner le disque maître (Drive 0, LBA mode activé) */
    outb(ATA_PRIMARY_IO + ATA_REG_DRIVE_SEL, 0xA0);
    io_wait();

    /* 2. Mettre à 0 les registres LBA & Sector Count */
    outb(ATA_PRIMARY_IO + ATA_REG_SECCOUNT, 0);
    outb(ATA_PRIMARY_IO + ATA_REG_LBA_LO, 0);
    outb(ATA_PRIMARY_IO + ATA_REG_LBA_MID, 0);
    outb(ATA_PRIMARY_IO + ATA_REG_LBA_HI, 0);

    /* 3. Envoyer la commande IDENTIFY */
    outb(ATA_PRIMARY_IO + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
    io_wait();

    uint8_t status = inb(ATA_PRIMARY_IO + ATA_REG_STATUS);
    if (status == 0) {
        serial_print("[-] IDE: Aucun disque detecte sur Primary Master.\n");
        return;
    }

    ata_wait_bsy();

    /* Vérifier si le disque est un périphérique non-ATA (ATAPI) */
    if (inb(ATA_PRIMARY_IO + ATA_REG_LBA_MID) != 0 || inb(ATA_PRIMARY_IO + ATA_REG_LBA_HI) != 0) {
        serial_print("[-] IDE: Peripherique non-ATA detecte (ATAPI CD-ROM).\n");
        return;
    }

    ata_wait_drq();

    /* 4. Lire les 256 mots (512 octets) de métadonnées */
    uint16_t identify_data[256];
    for (int i = 0; i < 256; i++) {
        identify_data[i] = inw(ATA_PRIMARY_IO + ATA_REG_DATA);
    }

    /* Secteurs LBA28 disponibles dans identify_data[60] et [61] */
    uint32_t total_sectors = ((uint32_t)identify_data[61] << 16) | identify_data[60];
    uint32_t size_mb = (total_sectors * 512) / (1024 * 1024);

    serial_print("[+] Disque ATA Detecte! Secteurs LBA28: ");
    serial_print_dec(total_sectors);
    serial_print(" (~");
    serial_print_dec(size_mb);
    serial_print(" MB)\n");
}

bool ide_read_sectors(uint32_t lba, uint8_t sector_count, uint8_t *buffer) {
    if (sector_count == 0) return false;

    ata_wait_bsy();

    /* Sélection du lecteur maître avec les 4 bits de poids fort de LBA */
    outb(ATA_PRIMARY_IO + ATA_REG_DRIVE_SEL, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_PRIMARY_IO + ATA_REG_SECCOUNT, sector_count);
    outb(ATA_PRIMARY_IO + ATA_REG_LBA_LO, (uint8_t)lba);
    outb(ATA_PRIMARY_IO + ATA_REG_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_PRIMARY_IO + ATA_REG_LBA_HI, (uint8_t)(lba >> 16));
    outb(ATA_PRIMARY_IO + ATA_REG_COMMAND, ATA_CMD_READ_PIO);

    uint16_t *buf16 = (uint16_t *)buffer;

    for (uint8_t s = 0; s < sector_count; s++) {
        ata_wait_bsy();
        ata_wait_drq();

        for (int i = 0; i < 256; i++) {
            *buf16++ = inw(ATA_PRIMARY_IO + ATA_REG_DATA);
        }
    }

    return true;
}

bool ide_write_sectors(uint32_t lba, uint8_t sector_count, const uint8_t *buffer) {
    if (sector_count == 0) return false;

    ata_wait_bsy();

    outb(ATA_PRIMARY_IO + ATA_REG_DRIVE_SEL, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_PRIMARY_IO + ATA_REG_SECCOUNT, sector_count);
    outb(ATA_PRIMARY_IO + ATA_REG_LBA_LO, (uint8_t)lba);
    outb(ATA_PRIMARY_IO + ATA_REG_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_PRIMARY_IO + ATA_REG_LBA_HI, (uint8_t)(lba >> 16));
    outb(ATA_PRIMARY_IO + ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);

    const uint16_t *buf16 = (const uint16_t *)buffer;

    for (uint8_t s = 0; s < sector_count; s++) {
        ata_wait_bsy();
        ata_wait_drq();

        for (int i = 0; i < 256; i++) {
            outw(ATA_PRIMARY_IO + ATA_REG_DATA, *buf16++);
        }
    }

    return true;
}
