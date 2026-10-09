global gdt_flush
section .text
bits 64

gdt_flush:
    lgdt [rdi]

    ; Recharger les segments de donnees avec le selecteur 0x10
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax

    ; Recharger CS avec 0x08 via un far return
    push qword 0x08
    lea rax, [rel .reload_cs]
    push rax
    retfq

.reload_cs:
    ret
