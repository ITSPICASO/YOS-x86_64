#ifndef E1000_H
#define E1000_H

#include <stdint.h>
#include <stdbool.h>

/* Registres MMIO Intel e1000 */
#define REG_CTRL        0x0000
#define REG_STATUS      0x0008
#define REG_EEPROM      0x0014
#define REG_ICR         0x00C0
#define REG_IMS         0x00D0
#define REG_RCTL        0x0100
#define REG_TCTL        0x0400

#define INTEL_VEND      0x8086
#define E1000_DEV       0x100E

typedef struct {
    uint64_t mmio_base;
    uint8_t mac[6];
    bool link_up;
} e1000_device_t;


/* Registres RX/TX Ring */
#define REG_RDBAL       0x2800
#define REG_RDBAH       0x2804
#define REG_RDLEN       0x2808
#define REG_RDH         0x2810
#define REG_RDT         0x2818

#define REG_TDBAL       0x3800
#define REG_TDBAH       0x3804
#define REG_TDLEN       0x3808
#define REG_TDH         0x3810
#define REG_TDT         0x3818

/* Bits RCTL */
#define RCTL_EN         (1 << 1)
#define RCTL_SBP        (1 << 2)
#define RCTL_UPE        (1 << 3)
#define RCTL_MPE        (1 << 4)
#define RCTL_BAM        (1 << 15)
#define RCTL_BSIZE_2048 (0 << 16)
#define RCTL_SECRC      (1 << 26)

/* Bits TCTL */
#define TCTL_EN         (1 << 1)
#define TCTL_PSP        (1 << 3)
#define TCTL_CT_SHIFT   4
#define TCTL_COLD_SHIFT 12

/* Tailles des Anneaux (Rings) */
#define NUM_RX_DESC     32
#define NUM_TX_DESC     32
#define RX_BUFFER_SIZE  2048

/* Descripteur de Reception (Legacy) */
typedef struct __attribute__((packed)) {
    uint64_t addr;
    uint16_t length;
    uint16_t checksum;
    uint8_t  status;
    uint8_t  errors;
    uint16_t special;
} e1000_rx_desc_t;

/* Descripteur de Transmission (Legacy) */
typedef struct __attribute__((packed)) {
    uint64_t addr;
    uint16_t length;
    uint8_t  cso;
    uint8_t  cmd;
    uint8_t  status;
    uint8_t  css;
    uint16_t special;
} e1000_tx_desc_t;

void e1000_poll_rx(void);
int e1000_send_packet(const void *data, uint16_t len);

#define E1000_IRQ_VECTOR 0x2B

void e1000_irq_handler(void);
extern void e1000_stub(void);

int e1000_init(void);
e1000_device_t *e1000_get_device(void);

#endif
