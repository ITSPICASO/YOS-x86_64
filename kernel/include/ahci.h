#ifndef AHCI_H
#define AHCI_H

#include <stdint.h>
#include <stdbool.h>

#define AHCI_PCI_CLASS      0x01
#define AHCI_PCI_SUBCLASS   0x06
#define AHCI_PCI_PROG_IF    0x01

#define HBA_PORT_IPM_ACTIVE 1
#define HBA_PORT_DET_PRESENT 3

#define SATA_SIG_ATA    0x00000101
#define SATA_SIG_ATAPI  0xEB140101
#define SATA_SIG_SEMB   0xC33C0101
#define SATA_SIG_PM     0x96690101

#define HBA_PxCMD_ST    0x0001
#define HBA_PxCMD_FRE   0x0010
#define HBA_PxCMD_FR    0x4000
#define HBA_PxCMD_CR    0x8000

#define ATA_CMD_READ_DMA_EX  0x25
#define ATA_CMD_WRITE_DMA_EX 0x35

/* PRDT (Physical Region Descriptor Table) Entry */
struct hba_prdt_entry {
    uint32_t dba;        /* Data Base Address low */
    uint32_t dbau;       /* Data Base Address upper 32 bits */
    uint32_t reserved;
    uint32_t dbc:22;     /* Byte count (0-indexed: 4MB max per entry, bit 0 doit être 1 pour 2 octets) */
    uint32_t reserved1:9;
    uint32_t i:1;        /* Interrupt on completion */
} __attribute__((packed));

/* Command Table */
struct hba_cmd_tbl {
    uint8_t cfis[64];    /* Command FIS */
    uint8_t acmd[16];    /* ATAPI Command */
    uint8_t reserved[48];
    struct hba_prdt_entry prdt_entry[1]; /* 1 entrée PRDT suffisante pour nos transferts */
} __attribute__((packed));

/* Command Header */
struct hba_cmd_header {
    uint8_t  cfl:5;      /* Command FIS length in DWORDS (ex: 5 pour H2D) */
    uint8_t  a:1;        /* ATAPI */
    uint8_t  w:1;        /* Write (1 = out to device, 0 = in from device) */
    uint8_t  p:1;        /* Prefetchable */
    uint8_t  r:1;        /* Reset */
    uint8_t  b:1;        /* BIST */
    uint8_t  c:1;        /* Clear busy upon R_OK */
    uint8_t  rsv0:1;
    uint8_t  pmp:4;      /* Port multiplier port */
    uint16_t prdtl;      /* PRDT length in entries */
    volatile uint32_t prdbc; /* PRD byte count transferred */
    uint32_t ctba;       /* Command Table Base Address low */
    uint32_t ctbau;      /* Command Table Base Address upper 32 bits */
    uint32_t rsv1[4];
} __attribute__((packed));

/* Port Memory Register Structure */
struct hba_port {
    uint32_t clb;        /* 0x00: Command List Base Address low */
    uint32_t clbu;       /* 0x04: Command List Base Address high */
    uint32_t fb;         /* 0x08: FIS Base Address low */
    uint32_t fbu;        /* 0x0C: FIS Base Address high */
    uint32_t is;         /* 0x10: Interrupt Status */
    uint32_t ie;         /* 0x14: Interrupt Enable */
    uint32_t cmd;        /* 0x18: Command and Status */
    uint32_t rsv0;       /* 0x1C */
    uint32_t tfd;        /* 0x20: Task File Data */
    uint32_t sig;        /* 0x24: Signature */
    uint32_t ssts;       /* 0x28: SATA Status */
    uint32_t sctl;       /* 0x2C: SATA Control */
    uint32_t serr;       /* 0x30: SATA Error */
    uint32_t sact;       /* 0x34: SATA Active */
    uint32_t ci;         /* 0x38: Command Issue */
    uint32_t sntf;       /* 0x3C: SATA Notification */
    uint32_t fbs;        /* 0x40: FIS-based switch control */
    uint32_t rsv1[11];
    uint32_t vendor[4];
} __attribute__((packed));

/* Generic Host Control (AHCI Base Memory Registers) */
struct hba_mem {
    uint32_t cap;        /* 0x00: Host Capabilities */
    uint32_t ghc;        /* 0x04: Global Host Control */
    uint32_t is;         /* 0x08: Interrupt Status */
    uint32_t pi;         /* 0x0C: Ports Implemented */
    uint32_t vs;         /* 0x10: Version */
    uint32_t ccc_ctl;    /* 0x14: Command Coalescing Control */
    uint32_t ccc_pts;    /* 0x18: Command Coalescing Ports */
    uint32_t em_loc;     /* 0x1C: Enclosure Management Location */
    uint32_t em_ctl;     /* 0x20: Enclosure Management Control */
    uint32_t cap2;       /* 0x24: Capabilities Extended */
    uint32_t bohc;       /* 0x28: BIOS/OS Handoff Control and Status */
    uint8_t  rsv[0xA0-0x2C];
    uint8_t  vendor[0x100-0xA0];
    struct hba_port ports[32];
} __attribute__((packed));

/* FIS Host-to-Device (H2D) */
struct fis_reg_h2d {
    uint8_t  fis_type;   /* 0x27 */
    uint8_t  pmport:4;
    uint8_t  rsv0:3;
    uint8_t  c:1;        /* 1 = Command, 0 = Control */
    uint8_t  command;    /* ATA Command */
    uint8_t  featurel;

    uint8_t  lba0;
    uint8_t  lba1;
    uint8_t  lba2;
    uint8_t  device;

    uint8_t  lba3;
    uint8_t  lba4;
    uint8_t  lba5;
    uint8_t  featureh;

    uint8_t  countl;
    uint8_t  counth;
    uint8_t  icc;
    uint8_t  control;

    uint8_t  rsv1[4];
} __attribute__((packed));

void ahci_init(void);
bool ahci_read_sectors(uint8_t port_no, uint64_t lba, uint32_t count, void *buf);
bool ahci_write_sectors(uint8_t port_no, uint64_t lba, uint32_t count, const void *buf);

#endif
