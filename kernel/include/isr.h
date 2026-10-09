#ifndef ISR_H
#define ISR_H

#include <stdint.h>

/* Structure représentant la pile lors de l'appel au gestionnaire C */
typedef struct {
    /* Registres sauvegardés manuellement (ordre inverse de push) */
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    
    /* Informations injectées par nos stubs assembleur */
    uint64_t int_no;
    uint64_t err_code;
    
    /* Poussés automatiquement par le CPU lors de l'interruption */
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} __attribute__((packed)) interrupt_frame_t;

typedef void (*isr_handler_t)(interrupt_frame_t *);

void isr_install(void);
void isr_register_handler(uint8_t n, isr_handler_t handler);

#endif