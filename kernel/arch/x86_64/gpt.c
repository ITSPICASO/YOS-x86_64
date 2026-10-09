#include "gpt.h"
#include "serial.h"

static const uint32_t crc32_tab[] = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA,
    0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3,
    0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
    0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91
};

uint32_t crc32(const void *data, size_t n_bytes) {
    const uint8_t *p = (const uint8_t *)data;
    uint32_t crc = ~0U;
    while (n_bytes--) {
        crc = (crc >> 4) ^ crc32_tab[(crc ^ (*p)) & 0x0F];
        crc = (crc >> 4) ^ crc32_tab[(crc ^ (*p >> 4)) & 0x0F];
        p++;
    }
    return ~crc;
}

int gpt_parse(const uint8_t *lba1_buffer) {
    serial_print("[*] Etape 53 : Analyse de la table de partition GPT...\n");
    const gpt_header_t *hdr = (const gpt_header_t *)lba1_buffer;

    /* Verifier signature "EFI PART" */
    const char *sig = "EFI PART";
    for (int i = 0; i < 8; i++) {
        if (hdr->signature[i] != sig[i]) {
            serial_print("[-] GPT : Signature invalide ou disque non-GPT.\n");
            return 0;
        }
    }

    serial_print("[+] GPT : Signature EFI PART valide detectee!\n");
    serial_print("  -> LBA du Header         : ");
    serial_print_dec((uint32_t)hdr->my_lba);
    serial_print("\n  -> Premier LBA utilisable: ");
    serial_print_dec((uint32_t)hdr->first_usable_lba);
    serial_print("\n  -> Dernier LBA utilisable: ");
    serial_print_dec((uint32_t)hdr->last_usable_lba);
    serial_print("\n  -> Nombre de partitions  : ");
    serial_print_dec(hdr->num_partition_entries);
    serial_print("\n");

    return 1;
}
