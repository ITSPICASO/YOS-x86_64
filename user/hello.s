[BITS 64]
global _start

section .text
_start:
    ; 1. sys_open("/welcome.txt", 0)
    mov rax, 4
    lea rdi, [rel file_path]
    mov rsi, 0
    syscall
    mov r13, rax           ; r13 = fd du fichier

    ; 2. sys_read(fd, file_buf, 64)
    mov rax, 2
    mov rdi, r13
    lea rsi, [rel file_buf]
    mov rdx, 64
    syscall
    mov r14, rax           ; r14 = taille lue

    ; 3. sys_write(1, prefix, prefix_len)
    mov rax, 3
    mov rdi, 1
    lea rsi, [rel prefix]
    mov rdx, prefix_len
    syscall

    ; 4. sys_write(1, file_buf, r14)
    mov rax, 3
    mov rdi, 1
    lea rsi, [rel file_buf]
    mov rdx, r14
    syscall

    ; 5. sys_close(fd)
    mov rax, 5
    mov rdi, r13
    syscall

    ; 6. Demande clavier
    mov rax, 3
    mov rdi, 1
    lea rsi, [rel prompt]
    mov rdx, prompt_len
    syscall

    mov rax, 2
    mov rdi, 0
    lea rsi, [rel user_input]
    mov rdx, 64
    syscall
    mov r12, rax

    mov rax, 3
    mov rdi, 1
    lea rsi, [rel reply]
    mov rdx, reply_len
    syscall

    mov rax, 3
    mov rdi, 1
    lea rsi, [rel user_input]
    mov rdx, r12
    syscall

    ; 7. sys_exit(0)
    mov rax, 0
    mov rdi, 0
    syscall

.halt:
    jmp .halt

section .data
file_path:  db "/welcome.txt", 0
prefix:     db 10, ">>> [Contenu Lu depuis Initrd TAR] : ", 0
prefix_len  equ $ - prefix

prompt:     db 10, ">>> [YOS Ring 3] Ktbet smitek: ", 0
prompt_len  equ $ - prompt

reply:      db ">>> [YOS Ring 3] Marhba bik ya: ", 0
reply_len   equ $ - reply

section .bss
file_buf:   resb 64
user_input: resb 64
