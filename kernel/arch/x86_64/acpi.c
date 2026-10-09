#include "acpi.h"
#include "serial.h"
#include <stddef.h>

#define HIGHER_HALF_OFFSET 0xFFFFFFFF80000000ULL

static uint64_t ioapic_phys_base = 0xFEC00000ULL;

static bool acpi_validate_checksum(const void *ptr, size_t len) {
    const uint8_t *bytes = (const uint8_t *)ptr;
    uint8_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum += bytes[i];
    }
    return sum == 0;
}

static struct acpi_rsdp *acpi_find_rsdp(void) {
    uint8_t *mem = (uint8_t *)(HIGHER_HALF_OFFSET + 0x000E0000ULL);
    for (uint64_t offset = 0; offset < 0x20000; offset += 16) {
        const char *sig = (const char *)&mem[offset];
        if (sig[0] == 'R' && sig[1] == 'S' && sig[2] == 'D' && sig[3] == ' ' &&
            sig[4] == 'P' && sig[5] == 'T' && sig[6] == 'R' && sig[7] == ' ') {
            if (acpi_validate_checksum(&mem[offset], sizeof(struct acpi_rsdp))) {
                return (struct acpi_rsdp *)&mem[offset];
            }
        }
    }
    return NULL;
}

static void acpi_parse_madt(struct acpi_madt *madt) {
    serial_print("[+] ACPI : Table MADT identifiee.\n");
    uint8_t *ptr = (uint8_t *)madt + sizeof(struct acpi_madt);
    uint8_t *end = (uint8_t *)madt + madt->header.length;

    while (ptr < end) {
        struct acpi_madt_record *record = (struct acpi_madt_record *)ptr;
        if (record->length == 0) break;

        if (record->type == 1) {
            struct madt_ioapic *io = (struct madt_ioapic *)ptr;
            ioapic_phys_base = io->ioapic_address;
            serial_print("  -> I/O APIC ID : ");
            serial_print_dec(io->ioapic_id);
            serial_print(" | Base : ");
            serial_print_hex(io->ioapic_address);
            serial_print("\n");
        } else if (record->type == 2) {
            struct madt_iso *iso = (struct madt_iso *)ptr;
            serial_print("  -> ISO IRQ ");
            serial_print_dec(iso->irq);
            serial_print(" -> GSI ");
            serial_print_dec(iso->gsi);
            serial_print("\n");
        }
        ptr += record->length;
    }
}

void acpi_init(uint64_t multiboot_addr) {
    (void)multiboot_addr;
    serial_print("[*] Scan ACPI RSDP...\n");

    struct acpi_rsdp *rsdp = acpi_find_rsdp();
    if (!rsdp) {
        serial_print("[-] ACPI: RSDP introuvable, fallback I/O APIC standard 0xFEC00000.\n");
        return;
    }

    serial_print("[+] ACPI: RSDP localise a ");
    serial_print_hex((uint64_t)rsdp);
    serial_print("\n");

    struct acpi_sdt_header *rsdt = (struct acpi_sdt_header *)(HIGHER_HALF_OFFSET + rsdp->rsdt_address);
    if (!acpi_validate_checksum(rsdt, rsdt->length)) {
        serial_print("[-] ACPI: Checksum RSDT invalide.\n");
        return;
    }

    size_t entries = (rsdt->length - sizeof(struct acpi_sdt_header)) / sizeof(uint32_t);
    uint32_t *table_pointers = (uint32_t *)((uint8_t *)rsdt + sizeof(struct acpi_sdt_header));

    for (size_t i = 0; i < entries; i++) {
        struct acpi_sdt_header *header = (struct acpi_sdt_header *)(HIGHER_HALF_OFFSET + table_pointers[i]);
        if (header->signature[0] == 'A' && header->signature[1] == 'P' &&
            header->signature[2] == 'I' && header->signature[3] == 'C') {
            acpi_parse_madt((struct acpi_madt *)header);
            return;
        }
    }
}

uint64_t acpi_get_ioapic_base(void) {
    return ioapic_phys_base;
}
