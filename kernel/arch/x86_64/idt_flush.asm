global idt_flush
section .text
bits 64

idt_flush:
    lidt [rdi]
    ret
