#ifndef ACPI_H
#define ACPI_H

#include <stdint.h>
#include <stdbool.h>

/* RSDP v1.0 (ACPI 1.0) */
struct acpi_rsdp {
    char     signature[8];
    uint8_t  checksum;
    char     oem_id[6];
    uint8_t  revision;
    uint32_t rsdt_address;
} __attribute__((packed));

/* RSDP v2.0+ (ACPI 2.0+) */
struct acpi_xsdp {
    struct acpi_rsdp rsdp;
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t  extended_checksum;
    uint8_t  reserved[3];
} __attribute__((packed));

/* En-tête standard SDT (System Description Table) */
struct acpi_sdt_header {
    char     signature[4];
    uint32_t length;
    uint8_t  revision;
    uint8_t  checksum;
    char     oem_id[6];
    char     oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed));

/* Table MADT (Multiple APIC Description Table) */
struct acpi_madt {
    struct acpi_sdt_header header;
    uint32_t lapic_addr;
    uint32_t flags;
} __attribute__((packed));

/* Entête d enregistrement MADT */
struct acpi_madt_record {
    uint8_t type;
    uint8_t length;
} __attribute__((packed));

/* Type 1 : I/O APIC */
struct madt_ioapic {
    struct acpi_madt_record header;
    uint8_t  ioapic_id;
    uint8_t  reserved;
    uint32_t ioapic_address;
    uint32_t gsi_base;
} __attribute__((packed));

/* Type 2 : Interrupt Source Override (ISO) */
struct madt_iso {
    struct acpi_madt_record header;
    uint8_t  bus;
    uint8_t  irq;
    uint32_t gsi;
    uint16_t flags;
} __attribute__((packed));

void acpi_init(uint64_t multiboot_addr);
uint64_t acpi_get_ioapic_base(void);

#endif
