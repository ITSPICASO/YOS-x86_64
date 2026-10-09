global tss_flush
section .text
bits 64

tss_flush:
    mov ax, 0x28 ; Index 5 f GDT (5 * 8 = 0x28)
    ltr ax       ; Load Task Register
    ret
