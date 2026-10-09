extern current_thread
[BITS 64]
global syscall_entry
global user_rsp_save
global user_rip_save
extern syscall_dispatcher

section .bss
align 16
user_rsp_save: resq 1
user_rip_save: resq 1
kernel_syscall_stack: resb 16384
kernel_syscall_stack_top:

section .text
syscall_entry:
    ; 1. Sauvegarder user RSP et user RIP
    ; R11 contient RFLAGS (fourni par CPU)
    ; RCX contient User RIP (fourni par CPU)
    ; RAX contient Syscall ID (NE PAS TOUCHER!)
    mov [rel user_rsp_save], rsp
    mov [rel user_rip_save], rcx

    ; Sauvegarder user_rsp dans current_thread->user_rsp (offset 8) sans toucher RAX
    ; On utilise R11 comme registre temporaire
    mov r11, [rel current_thread]
    test r11, r11
    jz .use_global_stack

    ; current_thread->user_rsp = user_rsp
    mov [r11 + 8], rsp

    ; Charger stack_base propre au thread (offset 40)
    mov r11, [r11 + 40]
    test r11, r11
    jz .use_global_stack

    add r11, 4096             ; Haut de la pile noyau dediee
    mov rsp, r11
    jmp .stack_ready

.use_global_stack:
    lea rsp, [rel kernel_syscall_stack_top]

.stack_ready:
    ; 2. Sauvegarder le contexte (RAX preserve intact!)
    ; On restaure R11 avec 0x202 ou la valeur initiale de flags
    push qword 0x202          ; user RFLAGS
    push rcx                  ; user RIP
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15
    push rdx
    push rsi
    push rdi
    push rax                  ; Syscall ID intact!

    ; 3. Tartib SysV ABI
    mov r9,  r8          ; a5
    mov r8,  r10         ; a4
    mov rcx, rdx         ; a3
    mov rdx, rsi         ; a2
    mov rsi, rdi         ; a1
    mov rdi, rax         ; num

    sub rsp, 8
    call syscall_dispatcher
    add rsp, 8

    ; Restaurer les registres
    add rsp, 8          ; saute l'ancien RAX (valeur de retour dans RAX intacte)
    pop rdi
    pop rsi
    pop rdx
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    pop rcx             ; user RIP
    pop r11             ; user RFLAGS (0x202)

    ; 4. Restaurer user RSP
    mov r11, [rel current_thread]
    test r11, r11
    jz .fallback
    mov rsp, [r11 + 8]
    mov r11, 0x202
    o64 sysret

.fallback:
    mov rsp, [rel user_rsp_save]
    mov r11, 0x202
    o64 sysret
