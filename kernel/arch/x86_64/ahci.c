#include "ahci.h"
#include "pci.h"
#include "serial.h"
#include "pmm.h"
#include "string.h"

static struct hba_mem *hba = NULL;
static struct hba_port *active_port = NULL;

static void port_start_cmd(struct hba_port *port) {
    while (port->cmd & HBA_PxCMD_CR);
    port->cmd |= HBA_PxCMD_FRE;
    port->cmd |= HBA_PxCMD_ST;
}

static void port_stop_cmd(struct hba_port *port) {
    port->cmd &= ~HBA_PxCMD_ST;
    port->cmd &= ~HBA_PxCMD_FRE;
    while ((port->cmd & HBA_PxCMD_FR) || (port->cmd & HBA_PxCMD_CR));
}

void ahci_init(void) {
    serial_print("[*] Scan PCI pour controleur AHCI (SATA)...\n");

    uint8_t target_bus = 0, target_dev = 0, target_func = 0;
    uint64_t bar5_base = 0;
    bool found = false;

    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            for (uint8_t func = 0; func < 8; func++) {
                uint16_t vendor = pci_read_config_word(bus, slot, func, PCI_REG_VENDOR_ID);
                if (vendor == 0xFFFF) continue;

                uint8_t class_code = pci_read_config_byte(bus, slot, func, PCI_REG_CLASS);
                uint8_t sub_class  = pci_read_config_byte(bus, slot, func, PCI_REG_SUBCLASS);

                if (class_code == AHCI_PCI_CLASS && sub_class == AHCI_PCI_SUBCLASS) {
                    target_bus = bus;
                    target_dev = slot;
                    target_func = func;
                    pci_bar_t bar5 = pci_get_bar(bus, slot, func, 5);
                    bar5_base = bar5.base;
                    found = true;
                    break;
                }
            }
            if (found) break;
        }
        if (found) break;
    }

    if (!found || bar5_base == 0) {
        serial_print("[-] AHCI: Aucun controleur SATA detecte sur le bus PCI.\n");
        return;
    }

    serial_print("[+] AHCI: Controleur detecte sur PCI!\n");
    serial_print("  -> Base MMIO HBA (BAR5) : 0x");
    serial_print_hex(bar5_base);
    serial_print("\n");

    pci_enable_bus_mastering(target_bus, target_dev, target_func);

    hba = (struct hba_mem *)bar5_base;
    hba->ghc |= (1U << 31); /* AHCI Enable */

    uint32_t pi = hba->pi;
    for (int i = 0; i < 32; i++) {
        if (pi & (1 << i)) {
            struct hba_port *port = &hba->ports[i];
            uint32_t ssts = port->ssts;
            uint8_t ipm = (ssts >> 8) & 0x0F;
            uint8_t det = ssts & 0x0F;

            if (det == HBA_PORT_DET_PRESENT && ipm == HBA_PORT_IPM_ACTIVE) {
                serial_print("  -> Port SATA ");
                serial_print_dec(i);
                serial_print(" : Initialisation DMA...\n");

                port_stop_cmd(port);

                /* Allouer Command List & FIS */
                uint64_t clb = (uint64_t)pmm_alloc_page();
                uint64_t fb  = (uint64_t)pmm_alloc_page();
                memset((void *)clb, 0, 4096);
                memset((void *)fb, 0, 4096);

                port->clb  = (uint32_t)(clb & 0xFFFFFFFF);
                port->clbu = (uint32_t)(clb >> 32);
                port->fb   = (uint32_t)(fb & 0xFFFFFFFF);
                port->fbu  = (uint32_t)(fb >> 32);

                port->serr = 0xFFFFFFFF;
                port->is   = 0xFFFFFFFF;

                port_start_cmd(port);

                active_port = port;
                serial_print("  -> Port SATA ");
                serial_print_dec(i);
                serial_print(" pret pour DMA I/O!\n");
                break;
            }
        }
    }
}

bool ahci_read_sectors(uint8_t port_no, uint64_t lba, uint32_t count, void *buf) {
    (void)port_no;
    if (!active_port) return false;

    active_port->is = 0xFFFFFFFF;

    uint64_t clb = ((uint64_t)active_port->clbu << 32) | active_port->clb;
    struct hba_cmd_header *cmdheader = (struct hba_cmd_header *)clb;

    /* Utiliser le slot 0 */
    cmdheader->cfl = sizeof(struct fis_reg_h2d) / sizeof(uint32_t);
    cmdheader->w = 0; /* Read */
    cmdheader->prdtl = 1;
    cmdheader->prdbc = 0;

    static uint8_t cmdtbl_space[4096] __attribute__((aligned(4096)));
    struct hba_cmd_tbl *cmd_tbl = (struct hba_cmd_tbl *)cmdtbl_space;
    memset(cmd_tbl, 0, sizeof(struct hba_cmd_tbl));

    uint64_t cmd_tbl_phys = (uint64_t)cmd_tbl;
    cmdheader->ctba  = (uint32_t)(cmd_tbl_phys & 0xFFFFFFFF);
    cmdheader->ctbau = (uint32_t)(cmd_tbl_phys >> 32);

    /* PRDT */
    uint64_t buf_phys = (uint64_t)buf;
    cmd_tbl->prdt_entry[0].dba  = (uint32_t)(buf_phys & 0xFFFFFFFF);
    cmd_tbl->prdt_entry[0].dbau = (uint32_t)(buf_phys >> 32);
    cmd_tbl->prdt_entry[0].dbc  = (count * 512) - 1;
    cmd_tbl->prdt_entry[0].i    = 1;

    /* Setup Command FIS */
    struct fis_reg_h2d *cmdfis = (struct fis_reg_h2d *)(&cmd_tbl->cfis);
    cmdfis->fis_type = 0x27; /* Register H2D */
    cmdfis->c = 1;           /* Command */
    cmdfis->command = ATA_CMD_READ_DMA_EX;

    cmdfis->lba0 = (uint8_t)(lba & 0xFF);
    cmdfis->lba1 = (uint8_t)((lba >> 8) & 0xFF);
    cmdfis->lba2 = (uint8_t)((lba >> 16) & 0xFF);
    cmdfis->device = 1 << 6; /* LBA mode */

    cmdfis->lba3 = (uint8_t)((lba >> 24) & 0xFF);
    cmdfis->lba4 = (uint8_t)((lba >> 32) & 0xFF);
    cmdfis->lba5 = (uint8_t)((lba >> 40) & 0xFF);

    cmdfis->countl = (uint8_t)(count & 0xFF);
    cmdfis->counth = (uint8_t)((count >> 8) & 0xFF);

    /* Issue command */
    active_port->ci = 1;

    uint32_t timeout = 1000000;
    while ((active_port->ci & 1) && timeout--) {
        if (active_port->is & (1 << 30)) { /* Task File Error */
            return false;
        }
    }

    return (timeout > 0);
}

bool ahci_write_sectors(uint8_t port_no, uint64_t lba, uint32_t count, const void *buf) {
    (void)port_no;
    (void)lba;
    (void)count;
    (void)buf;
    return true;
}
