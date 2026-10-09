[bits 64]

extern isr_handler_dispatch

; Macro pour les exceptions sans code d'erreur automatique (on pousse 0)
%macro ISR_NOERRCODE 1
global isr%1
isr%1:
    push qword 0         ; Code d'erreur factice
    push qword %1        ; Numéro de l'interruption
    jmp isr_common_stub
%endmacro

; Macro pour les exceptions AVEC code d'erreur poussé par le CPU
%macro ISR_ERRCODE 1
global isr%1
isr%1:
    push qword %1        ; Numéro de l'interruption (le code d'erreur est déjà sur la pile)
    jmp isr_common_stub
%endmacro

; Définition des 32 ISRs x86_64
ISR_NOERRCODE 0   ; Divide-by-zero Error
ISR_NOERRCODE 1   ; Debug
ISR_NOERRCODE 2   ; Non-Maskable Interrupt (NMI)
ISR_NOERRCODE 3   ; Breakpoint
ISR_NOERRCODE 4   ; Overflow
ISR_NOERRCODE 5   ; Bound Range Exceeded
ISR_NOERRCODE 6   ; Invalid Opcode
ISR_NOERRCODE 7   ; Device Not Available
ISR_ERRCODE   8   ; Double Fault (avec code d'erreur)
ISR_NOERRCODE 9   ; Coprocessor Segment Overrun
ISR_ERRCODE   10  ; Invalid TSS (avec code d'erreur)
ISR_ERRCODE   11  ; Segment Not Present (avec code d'erreur)
ISR_ERRCODE   12  ; Stack-Segment Fault (avec code d'erreur)
ISR_ERRCODE   13  ; General Protection Fault (avec code d'erreur)
ISR_ERRCODE   14  ; Page Fault (avec code d'erreur)
ISR_NOERRCODE 15  ; Spurious / Reserved
ISR_NOERRCODE 16  ; x87 Floating-Point Exception
ISR_ERRCODE   17  ; Alignment Check (avec code d'erreur)
ISR_NOERRCODE 18  ; Machine Check
ISR_NOERRCODE 19  ; SIMD Floating-Point Exception
ISR_NOERRCODE 20  ; Virtualization Exception
ISR_ERRCODE   21  ; Control Protection Exception (avec code d'erreur)
ISR_NOERRCODE 22  ; Reserved
ISR_NOERRCODE 23  ; Reserved
ISR_NOERRCODE 24  ; Reserved
ISR_NOERRCODE 25  ; Reserved
ISR_NOERRCODE 26  ; Reserved
ISR_NOERRCODE 27  ; Reserved
ISR_NOERRCODE 28  ; Hypervisor Injection Exception
ISR_ERRCODE   29  ; VMM Communication Exception (avec code d'erreur)
ISR_ERRCODE   30  ; Security Exception (avec code d'erreur)
ISR_NOERRCODE 31  ; Reserved

isr_common_stub:
    ; Sauvegarde des registres généraux
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

    ; Selon la convention System V AMD64 ABI, le 1er argument passe dans RDI
    mov rdi, rsp
    call isr_handler_dispatch

    ; Restauration des registres généraux
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

    ; Nettoyage du numéro d'interruption et du code d'erreur (2 * 8 = 16 octets)
    add rsp, 16

    ; Retour d'interruption en mode 64-bit
    iretq