#include "mbr.h"
#include "serial.h"

void mbr_parse(const uint8_t *sector_data) {
    const struct mbr_sector *mbr = (const struct mbr_sector *)sector_data;

    serial_print("[*] Analyse du Master Boot Record (Secteur LBA 0)...\n");

    if (mbr->signature != MBR_SIGNATURE) {
        serial_print("[-] MBR: Signature invalide (0x");
        serial_print_hex(mbr->signature);
        serial_print(" != 0xAA55). Disque non partitionne en MBR ou vierge.\n");
        return;
    }

    serial_print("[+] MBR: Signature valide (0xAA55). Table des partitions:\n");

    for (int i = 0; i < 4; i++) {
        const struct mbr_entry *entry = &mbr->partitions[i];
        if (entry->type == 0x00 || entry->sector_count == 0) {
            continue;
        }

        serial_print("  -> Partition ");
        serial_print_dec(i + 1);
        serial_print(entry->attributes == 0x80 ? " [BOOTABLE]" : " [INACTIF] ");
        serial_print(" | Type: 0x");
        serial_print_hex(entry->type);
        serial_print(" | LBA Start: ");
        serial_print_dec(entry->lba_start);
        serial_print(" | Secteurs: ");
        serial_print_dec(entry->sector_count);
        serial_print(" (~");
        serial_print_dec((entry->sector_count * 512) / (1024 * 1024));
        serial_print(" MB)\n");
    }
}
