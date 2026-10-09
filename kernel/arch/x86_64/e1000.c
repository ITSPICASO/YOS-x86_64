#define KERNEL_VIRT_TO_PHYS(addr) ((uint64_t)(addr) - 0xFFFFFFFF80000000ULL)
#include "net.h"
#include "idt.h"
#include "ioapic.h"
#include "apic.h"
#include "pmm.h"
#include "heap.h"
#include "e1000.h"
#include "pci.h"
#include "serial.h"

static e1000_device_t net_dev;

static inline void e1000_write32(uint32_t reg, uint32_t val) {
    *((volatile uint32_t *)(net_dev.mmio_base + reg)) = val;
}

static inline uint32_t e1000_read32(uint32_t reg) {
    return *((volatile uint32_t *)(net_dev.mmio_base + reg));
}

static uint16_t e1000_read_eeprom(uint8_t addr) {
    uint32_t tmp = 0;
    e1000_write32(REG_EEPROM, 1 | ((uint32_t)(addr) << 8));
    while (!((tmp = e1000_read32(REG_EEPROM)) & (1 << 4)));
    return (uint16_t)((tmp >> 16) & 0xFFFF);
}

static void e1000_read_mac(void) {
    uint16_t val0 = e1000_read_eeprom(0);
    uint16_t val1 = e1000_read_eeprom(1);
    uint16_t val2 = e1000_read_eeprom(2);

    net_dev.mac[0] = val0 & 0xFF;
    net_dev.mac[1] = val0 >> 8;
    net_dev.mac[2] = val1 & 0xFF;
    net_dev.mac[3] = val1 >> 8;
    net_dev.mac[4] = val2 & 0xFF;
    net_dev.mac[5] = val2 >> 8;
}


/* Buffers alignes pour le DMA */
static e1000_rx_desc_t rx_descs[NUM_RX_DESC] __attribute__((aligned(16)));
static e1000_tx_desc_t tx_descs[NUM_TX_DESC] __attribute__((aligned(16)));
static uint8_t rx_buffers[NUM_RX_DESC][RX_BUFFER_SIZE] __attribute__((aligned(16)));

static uint16_t rx_cur = 0;
static uint16_t tx_cur = 0;

static void e1000_init_rx(void) {
    for (int i = 0; i < NUM_RX_DESC; i++) {
        rx_descs[i].addr = KERNEL_VIRT_TO_PHYS(&rx_buffers[i][0]);
        rx_descs[i].status = 0;
    }

    uint64_t rx_base_phys = KERNEL_VIRT_TO_PHYS(&rx_descs[0]);

    e1000_write32(REG_RDBAL, (uint32_t)(rx_base_phys & 0xFFFFFFFF));
    e1000_write32(REG_RDBAH, (uint32_t)(rx_base_phys >> 32));
    e1000_write32(REG_RDLEN, NUM_RX_DESC * sizeof(e1000_rx_desc_t));
    e1000_write32(REG_RDH, 0);
    e1000_write32(REG_RDT, NUM_RX_DESC - 1);

    /* Activation du recepteur : Broadcast Accept, 2048B buffer, Strip CRC */
    e1000_write32(REG_RCTL, RCTL_EN | RCTL_SBP | RCTL_UPE | RCTL_MPE | RCTL_BAM | RCTL_BSIZE_2048 | RCTL_SECRC);
}

static void e1000_init_tx(void) {
    for (int i = 0; i < NUM_TX_DESC; i++) {
        tx_descs[i].addr = 0;
        tx_descs[i].cmd = 0;
        tx_descs[i].status = (1 << 0); /* TXD_STAT_DD (Descriptor Done) */
    }

    uint64_t tx_base_phys = KERNEL_VIRT_TO_PHYS(&tx_descs[0]);

    e1000_write32(REG_TDBAL, (uint32_t)(tx_base_phys & 0xFFFFFFFF));
    e1000_write32(REG_TDBAH, (uint32_t)(tx_base_phys >> 32));
    e1000_write32(REG_TDLEN, NUM_TX_DESC * sizeof(e1000_tx_desc_t));
    e1000_write32(REG_TDH, 0);
    e1000_write32(REG_TDT, 0);

    /* Activation du transmetteur : Enable, Pad Short Packets, Collisions */
    e1000_write32(REG_TCTL, TCTL_EN | TCTL_PSP | (15 << TCTL_CT_SHIFT) | (64 << TCTL_COLD_SHIFT));
}

int e1000_send_packet(const void *data, uint16_t len) {
    if (!data || len == 0 || len > 1518) return -1;

    /* Timeout de securite pour ne jamais bloquer le noyau */
    uint32_t timeout = 100000;
    while (!(tx_descs[tx_cur].status & 0x01) && --timeout);

    static uint8_t tx_pkt_buffers[NUM_TX_DESC][1536] __attribute__((aligned(16)));
    for (uint16_t i = 0; i < len; i++) {
        tx_pkt_buffers[tx_cur][i] = ((const uint8_t *)data)[i];
    }

    tx_descs[tx_cur].addr = KERNEL_VIRT_TO_PHYS(&tx_pkt_buffers[tx_cur][0]);
    tx_descs[tx_cur].length = len;
    /* CMD: EOP (bit 0) | IFCS (bit 1) | RS (bit 3 = Report Status) */
    tx_descs[tx_cur].cmd = (1 << 0) | (1 << 1) | (1 << 3);
    tx_descs[tx_cur].status = 0;

    tx_cur = (tx_cur + 1) % NUM_TX_DESC;
    e1000_write32(REG_TDT, tx_cur);

    return 0;
}

e1000_device_t *e1000_get_device(void) {
    return &net_dev;
}


/* Reception des paquets (Polling / Interrupt) */
static void e1000_handle_receive(void) {
    while (rx_descs[rx_cur].status & 0x01) { /* DD: Descriptor Done */
        uint8_t *pkt = (uint8_t *)rx_descs[rx_cur].addr;
        uint16_t len = rx_descs[rx_cur].length;

        serial_print("[+] e1000: Paquet Ethernet recu! Taille: ");
        serial_print_dec(len);
        serial_print(" octets\n");
        net_handle_packet(pkt, len);

        /* Reset descripteur */
        rx_descs[rx_cur].status = 0;
        uint16_t old_cur = rx_cur;
        rx_cur = (rx_cur + 1) % NUM_RX_DESC;
        e1000_write32(REG_RDT, old_cur);
    }
}

void e1000_poll_rx(void) {
    e1000_handle_receive();
}

void e1000_irq_handler(void) {
    uint32_t icr = e1000_read32(REG_ICR);

    if (icr & (1 << 2)) {
        /* Link Status Change */
        uint32_t status = e1000_read32(REG_STATUS);
        net_dev.link_up = (status & (1 << 1)) != 0;
        serial_print("[*] e1000 IRQ: Link status modifie.\n");
    }

    if (icr & ((1 << 7) | (1 << 4))) {
        /* Packet RX Timer ou RX Overrun */
        e1000_handle_receive();
    }

    lapic_eoi();
}

int e1000_init(void) {
    serial_print("[*] Etape 87 : Initialisation du controleur Intel e1000...\n");

    bool found = false;
    uint8_t target_bus = 0, target_dev = 0, target_func = 0;

    /* Scan rapide du bus PCI pour detecter l e1000 */
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t dev = 0; dev < 32; dev++) {
            for (uint8_t func = 0; func < 8; func++) {
                uint16_t vend = pci_read_config_word((uint8_t)bus, dev, func, PCI_REG_VENDOR_ID);
                if (vend == 0xFFFF) {
                    if (func == 0) break;
                    continue;
                }
                uint16_t device = pci_read_config_word((uint8_t)bus, dev, func, PCI_REG_DEVICE_ID);
                if (vend == INTEL_VEND && device == E1000_DEV) {
                    target_bus = (uint8_t)bus;
                    target_dev = dev;
                    target_func = func;
                    found = true;
                    break;
                }
            }
            if (found) break;
        }
        if (found) break;
    }

    if (!found) {
        serial_print("[-] e1000: Carte reseau non detectee sur le bus PCI.\n");
        return -1;
    }

    /* Activation du Bus Mastering (DMA) & Memory Space */
    pci_enable_bus_mastering(target_bus, target_dev, target_func);

    /* Recuperation du BAR0 (MMIO) */
    pci_bar_t bar0 = pci_get_bar(target_bus, target_dev, target_func, 0);
    uint64_t mmio_phys = bar0.base;
    net_dev.mmio_base = mmio_phys; /* MMIO Identity-mapped 3GB-4GB */ /* Higher-Half direct map */

    /* Lecture de l adresse MAC depuis l EEPROM */
    e1000_read_mac();

    serial_print("[+] e1000: Carte detectee avec succes!\n");
    serial_print("    -> MMIO Base : 0x");
    serial_print_hex(mmio_phys);
    serial_print("\n    -> MAC Addr  : ");
    for (int i = 0; i < 6; i++) {
        serial_print_hex(net_dev.mac[i]);
        if (i < 5) serial_print(":");
    }
    serial_print("\n");

    /* Verification du Link Status */
    uint32_t status = e1000_read32(REG_STATUS);
    net_dev.link_up = (status & (1 << 1)) != 0;
    if (net_dev.link_up) {
        serial_print("[+] e1000: Link Up (Connexion reseau active)!\n");
    } else {
        serial_print("[*] e1000: Link Down (En attente du lien reseau).\n");
    }

    /* Initialisation des Anneaux DMA RX et TX */
    e1000_init_rx();
    e1000_init_tx();
    serial_print("[+] e1000: Anneaux DMA RX/TX (Ring Buffers) configures et actifs!\n");

    /* Configuration de l IRQ 11 */
    idt_set_gate(E1000_IRQ_VECTOR, (uint64_t)e1000_stub, 0x08, 0x8E, 0);
    ioapic_set_irq(11, E1000_IRQ_VECTOR, 0);
    ioapic_unmask_irq(11);

    /* Activation des interruptions e1000 (RXT0, RXO, RXDMT0, LSC) */
    e1000_write32(REG_IMS, 0x1F6DC);
    e1000_read32(REG_ICR); /* Clear pending */

    serial_print("[+] e1000: IRQ 11 routee sur Vecteur 0x2B via IOAPIC (Interruptions armees)!\n");

    return 0;
}
