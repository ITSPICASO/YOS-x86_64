bits 64
section .text
global apic_timer_stub
extern task_schedule_from_timer

apic_timer_stub:
    ; 1. Sauvegarder les 15 registres generaux
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; 2. Passer RSP actuel comme argument RDI
    mov rdi, rsp

    ; 3. Aligner la pile sur 16 octets avant CALL (160 octets pushed, sub rsp 8 pour aligner apres le push du call)
    sub rsp, 8
    call task_schedule_from_timer
    add rsp, 8

    ; 4. Basculer vers le nouveau RSP retourne dans RAX
    mov rsp, rax

    ; 5. Restaurer les registres
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    iretq
