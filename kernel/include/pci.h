#ifndef PCI_H
#define PCI_H

#include <stdint.h>
#include <stdbool.h>

#define PCI_CONFIG_ADDRESS      0xCF8
#define PCI_CONFIG_DATA         0xCFC

/* Registres standard de l'espace de configuration */
#define PCI_REG_VENDOR_ID       0x00
#define PCI_REG_DEVICE_ID       0x02
#define PCI_REG_COMMAND         0x04
#define PCI_REG_STATUS          0x06
#define PCI_REG_REVISION        0x08
#define PCI_REG_PROG_IF         0x09
#define PCI_REG_SUBCLASS        0x0A
#define PCI_REG_CLASS           0x0B
#define PCI_REG_HEADER_TYPE     0x0E
#define PCI_REG_BAR0            0x10
#define PCI_REG_INTERRUPT_LINE  0x3C

/* Bits du registre Command */
#define PCI_COMMAND_IO          (1 << 0)
#define PCI_COMMAND_MEMORY      (1 << 1)
#define PCI_COMMAND_MASTER      (1 << 2)
#define PCI_COMMAND_INT_DISABLE (1 << 10)

/* Type de BAR */
#define PCI_BAR_TYPE_MMIO       0
#define PCI_BAR_TYPE_IO         1

typedef struct {
    uint64_t base;
    uint32_t size;
    uint8_t  type;
    bool     prefetchable;
} pci_bar_t;

typedef struct {
    uint8_t   bus;
    uint8_t   device;
    uint8_t   function;
    uint16_t  vendor_id;
    uint16_t  device_id;
    uint8_t   class_code;
    uint8_t   subclass;
    uint8_t   prog_if;
    uint8_t   irq;
    pci_bar_t bars[6];
} pci_device_t;

uint32_t pci_read_config_dword(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset);
uint16_t pci_read_config_word(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset);
uint8_t  pci_read_config_byte(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset);

void     pci_write_config_dword(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint32_t val);
void     pci_write_config_word(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint16_t val);

void     pci_enable_bus_mastering(uint8_t bus, uint8_t dev, uint8_t func);
pci_bar_t pci_get_bar(uint8_t bus, uint8_t dev, uint8_t func, uint8_t bar_index);

void     pci_scan(void);

#endif
