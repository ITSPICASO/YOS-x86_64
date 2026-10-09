[BITS 64]
global _start

section .text
_start:
    ; 1. sys_write: Debut du test
    mov rax, 3
    mov rdi, 1
    lea rsi, [rel msg_start]
    mov rdx, msg_start_len
    syscall

    ; 2. sys_fork()
    mov rax, 1
    syscall

    test rax, rax
    jz .child_code

.parent_code:
    mov r12, rax           ; r12 = child PID

    ; Afficher message parent
    mov rax, 3
    mov rdi, 1
    lea rsi, [rel msg_parent]
    mov rdx, msg_parent_len
    syscall

    ; sys_waitpid(r12, &status, 0)
    mov rax, 6
    mov rdi, r12
    lea rsi, [rel status]
    mov rdx, 0
    syscall

    ; Afficher fin parent
    mov rax, 3
    mov rdi, 1
    lea rsi, [rel msg_parent_done]
    mov rdx, msg_parent_done_len
    syscall

    ; sys_exit(0)
    mov rax, 0
    mov rdi, 0
    syscall

.child_code:
    ; Afficher message enfant
    mov rax, 3
    mov rdi, 1
    lea rsi, [rel msg_child]
    mov rdx, msg_child_len
    syscall

    ; Yield pour laisser tourner un peu
    mov rax, 10
    syscall

    ; Afficher fin enfant
    mov rax, 3
    mov rdi, 1
    lea rsi, [rel msg_child_done]
    mov rdx, msg_child_done_len
    syscall

    ; sys_exit(42)
    mov rax, 0
    mov rdi, 42
    syscall

.halt:
    jmp .halt

section .data
msg_start:       db 10, ">>> [FORK TEST] Lancement dyal sys_fork()...", 10, 0
msg_start_len    equ $ - msg_start

msg_parent:      db ">>> [PARENT] Kheddam, kantsenna l-child b sys_waitpid()...", 10, 0
msg_parent_len   equ $ - msg_parent

msg_parent_done: db ">>> [PARENT] Child sala w t-netha! sys_exit(0)", 10, 0
msg_parent_done_len equ $ - msg_parent_done

msg_child:       db ">>> [CHILD] Salam! Ana l-child process f Ring 3!", 10, 0
msg_child_len    equ $ - msg_child

msg_child_done:  db ">>> [CHILD] Salit khedmti, sys_exit(42)", 10, 0
msg_child_done_len equ $ - msg_child_done

section .bss
status: resd 1
