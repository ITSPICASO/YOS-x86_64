[BITS 64]
global _start
extern main
extern exit

section .text
_start:
    ; Standard System V ABI: rdi = argc, rsi = argv
    xor rdi, rdi
    xor rsi, rsi
    call main
    mov rdi, rax
    call exit

.halt:
    jmp .halt
