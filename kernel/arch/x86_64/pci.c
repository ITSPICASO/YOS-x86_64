#include "pci.h"
#include "io.h"
#include "serial.h"

uint32_t pci_read_config_dword(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)((1U << 31) |
                                 ((uint32_t)bus << 16) |
                                 ((uint32_t)dev << 11) |
                                 ((uint32_t)func << 8) |
                                 (offset & 0xFC));
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

uint16_t pci_read_config_word(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset) {
    uint32_t dword = pci_read_config_dword(bus, dev, func, offset);
    return (uint16_t)((dword >> ((offset & 2) * 8)) & 0xFFFF);
}

uint8_t pci_read_config_byte(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset) {
    uint32_t dword = pci_read_config_dword(bus, dev, func, offset);
    return (uint8_t)((dword >> ((offset & 3) * 8)) & 0xFF);
}

void pci_write_config_dword(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint32_t val) {
    uint32_t address = (uint32_t)((1U << 31) |
                                 ((uint32_t)bus << 16) |
                                 ((uint32_t)dev << 11) |
                                 ((uint32_t)func << 8) |
                                 (offset & 0xFC));
    outl(PCI_CONFIG_ADDRESS, address);
    outl(PCI_CONFIG_DATA, val);
}

void pci_write_config_word(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint16_t val) {
    uint32_t old = pci_read_config_dword(bus, dev, func, offset);
    uint32_t shift = (offset & 2) * 8;
    uint32_t mask = 0xFFFF << shift;
    uint32_t new_val = (old & ~mask) | ((uint32_t)val << shift);
    pci_write_config_dword(bus, dev, func, offset, new_val);
}

void pci_enable_bus_mastering(uint8_t bus, uint8_t dev, uint8_t func) {
    uint16_t cmd = pci_read_config_word(bus, dev, func, PCI_REG_COMMAND);
    cmd |= (PCI_COMMAND_MASTER | PCI_COMMAND_MEMORY | PCI_COMMAND_IO);
    pci_write_config_word(bus, dev, func, PCI_REG_COMMAND, cmd);
}

pci_bar_t pci_get_bar(uint8_t bus, uint8_t dev, uint8_t func, uint8_t bar_index) {
    pci_bar_t bar = {0, 0, 0, false};
    if (bar_index > 5) return bar;

    uint8_t offset = PCI_REG_BAR0 + (bar_index * 4);
    uint32_t bar_low = pci_read_config_dword(bus, dev, func, offset);

    /* Calcul de la taille en écrivant tous les bits à 1 */
    pci_write_config_dword(bus, dev, func, offset, 0xFFFFFFFF);
    uint32_t size_mask = pci_read_config_dword(bus, dev, func, offset);
    pci_write_config_dword(bus, dev, func, offset, bar_low); /* Restaurer la valeur */

    if (size_mask == 0 || size_mask == 0xFFFFFFFF) return bar;

    if (bar_low & 0x01) {
        /* BAR I/O Port */
        bar.type = PCI_BAR_TYPE_IO;
        bar.base = bar_low & ~0x03;
        bar.size = ~(size_mask & ~0x03) + 1;
    } else {
        /* BAR Memory Mapped */
        bar.type = PCI_BAR_TYPE_MMIO;
        bar.prefetchable = (bar_low & (1 << 3)) != 0;
        uint8_t mem_type = (bar_low >> 1) & 0x03;

        if (mem_type == 0x02) {
            /* 64-bit BAR */
            uint32_t bar_high = pci_read_config_dword(bus, dev, func, offset + 4);
            bar.base = ((uint64_t)bar_high << 32) | (bar_low & ~0x0F);

            pci_write_config_dword(bus, dev, func, offset + 4, 0xFFFFFFFF);
            uint32_t size_high = pci_read_config_dword(bus, dev, func, offset + 4);
            pci_write_config_dword(bus, dev, func, offset + 4, bar_high);

            uint64_t full_mask = ((uint64_t)size_high << 32) | (size_mask & ~0x0F);
            bar.size = ~full_mask + 1;
        } else {
            /* 32-bit BAR */
            bar.base = bar_low & ~0x0F;
            bar.size = ~(size_mask & ~0x0F) + 1;
        }
    }
    return bar;
}

static void pci_check_function(uint8_t bus, uint8_t dev, uint8_t func) {
    uint16_t vendor_id = pci_read_config_word(bus, dev, func, PCI_REG_VENDOR_ID);
    if (vendor_id == 0xFFFF) return;

    uint16_t device_id = pci_read_config_word(bus, dev, func, PCI_REG_DEVICE_ID);
    uint32_t rev_class = pci_read_config_dword(bus, dev, func, PCI_REG_REVISION);
    uint8_t class_code = (uint8_t)((rev_class >> 24) & 0xFF);
    uint8_t subclass   = (uint8_t)((rev_class >> 16) & 0xFF);
    (void)rev_class;
    uint8_t irq_line   = pci_read_config_byte(bus, dev, func, PCI_REG_INTERRUPT_LINE);

    /* Activation du bus mastering */
    pci_enable_bus_mastering(bus, dev, func);

    serial_print("[PCI] ");
    serial_print_hex(bus);
    serial_print(":");
    serial_print_hex(dev);
    serial_print(".");
    serial_print_dec(func);
    serial_print(" | Vendor: ");
    serial_print_hex(vendor_id);
    serial_print(" Device: ");
    serial_print_hex(device_id);
    serial_print(" | Class: ");
    serial_print_hex(class_code);
    serial_print(" Sub: ");
    serial_print_hex(subclass);
    serial_print(" IRQ: ");
    serial_print_dec(irq_line);
    serial_print("\n");

    /* Analyse des BARs */
    for (uint8_t b = 0; b < 6; b++) {
        pci_bar_t bar = pci_get_bar(bus, dev, func, b);
        if (bar.size > 0) {
            serial_print("   -> BAR");
            serial_print_dec(b);
            serial_print(bar.type == PCI_BAR_TYPE_IO ? " (IO)  : " : " (MMIO): ");
            serial_print_hex(bar.base);
            serial_print(" | Taille: ");
            serial_print_hex(bar.size);
            serial_print("\n");
        }
    }
}

static void pci_check_device(uint8_t bus, uint8_t dev) {
    uint16_t vendor_id = pci_read_config_word(bus, dev, 0, PCI_REG_VENDOR_ID);
    if (vendor_id == 0xFFFF) return;

    pci_check_function(bus, dev, 0);

    uint16_t header_type = pci_read_config_word(bus, dev, 0, PCI_REG_HEADER_TYPE) & 0xFF;
    if (header_type & 0x80) {
        for (uint8_t func = 1; func < 8; func++) {
            if (pci_read_config_word(bus, dev, func, PCI_REG_VENDOR_ID) != 0xFFFF) {
                pci_check_function(bus, dev, func);
            }
        }
    }
}

void pci_scan(void) {
    serial_print("[*] Bloc 7 : Scan PCI avec decodage des BARs & Bus Mastering...\n");
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t dev = 0; dev < 32; dev++) {
            pci_check_device((uint8_t)bus, dev);
        }
    }
    serial_print("[+] Bloc 7 : Scan termine.\n");
}
