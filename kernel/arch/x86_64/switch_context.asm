bits 64
section .text
global switch_context
global enter_user_mode

switch_context:
    ; 1. Sauvegarder les registres callee-saved sur la pile actuelle
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15
    pushfq              ; Sauvegarder RFLAGS

    ; 2. Enregistrer l'ancien RSP dans *old_rsp
    mov [rdi], rsp

    ; 3. Charger le nouveau RSP depuis new_rsp
    mov rsp, rsi

    ; 4. Restaurer les registres callee-saved de la nouvelle tâche
    popfq               ; Restaurer RFLAGS
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx

    ; 5. Sauter vers le RIP sauvegardé
    ret

enter_user_mode:
    ; rdi = entry_point (RIP)
    ; rsi = user_rsp    (RSP)

    cli

    mov ax, 0x1B            ; User Data selector (Index 3: 0x18 | 3 = 0x1B)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push 0x1B               ; SS (User Data: 0x1B)
    push rsi                ; RSP (User Stack)
    push 0x202              ; RFLAGS (IF=1)
    push 0x23               ; CS (User Code: Index 4: 0x20 | 3 = 0x23)
    push rdi                ; RIP (ELF entry point)

    iretq
