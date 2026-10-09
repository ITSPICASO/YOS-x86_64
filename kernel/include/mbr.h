#ifndef MBR_H
#define MBR_H

#include <stdint.h>
#include <stdbool.h>

#define MBR_SIGNATURE 0xAA55

struct mbr_entry {
    uint8_t  attributes;   /* 0x80 = Active / Bootable */
    uint8_t  chs_start[3];
    uint8_t  type;         /* Type de partition (ex: 0x0C = FAT32 LBA, 0x83 = Linux) */
    uint8_t  chs_end[3];
    uint32_t lba_start;    /* Premier secteur LBA */
    uint32_t sector_count; /* Nombre de secteurs */
} __attribute__((packed));

struct mbr_sector {
    uint8_t           boot_code[446];
    struct mbr_entry  partitions[4];
    uint16_t          signature;
} __attribute__((packed));

void mbr_parse(const uint8_t *sector_data);

#endif
