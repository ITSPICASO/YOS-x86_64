[BITS 64]
global _start

section .text
_start:
    ; 1. Message de debut
    mov rax, 3          ; sys_write
    mov rdi, 1          ; stdout
    lea rsi, [rel msg_start]
    mov rdx, msg_start_len
    syscall

    ; 2. sys_socket(AF_INET=2, SOCK_DGRAM=2, 0)
    mov rax, 20         ; SYS_SOCKET
    mov rdi, 2          ; AF_INET
    mov rsi, 2          ; SOCK_DGRAM
    mov rdx, 0
    syscall

    cmp rax, 0
    jl .sock_error

    mov r12, rax        ; r12 = sockfd

    mov rax, 3
    mov rdi, 1
    lea rsi, [rel msg_sock_ok]
    mov rdx, msg_sock_ok_len
    syscall

    ; 3. sys_sendto(sockfd, buf, len, flags, &dest_addr)
    ; struct sockaddr_in {
    ;   uint16_t sin_family; // 2
    ;   uint16_t sin_port;   // htons(9999) = 0x0F27
    ;   uint32_t sin_addr;   // 10.0.2.2 = 0x0202000A
    ;   char sin_zero[8];
    ; }
    mov rax, 21         ; SYS_SENDTO
    mov rdi, r12        ; sockfd
    lea rsi, [rel payload]
    mov rdx, payload_len
    mov r10, 0          ; flags
    lea r8, [rel dest_addr] ; dest_addr
    syscall

    cmp rax, 0
    jl .send_error

    mov rax, 3
    mov rdi, 1
    lea rsi, [rel msg_send_ok]
    mov rdx, msg_send_ok_len
    syscall

    ; 4. sys_exit(0)
    mov rax, 0
    mov rdi, 0
    syscall

.sock_error:
    mov rax, 3
    mov rdi, 1
    lea rsi, [rel err_sock]
    mov rdx, err_sock_len
    syscall
    mov rax, 0
    mov rdi, 1
    syscall

.send_error:
    mov rax, 3
    mov rdi, 1
    lea rsi, [rel err_send]
    mov rdx, err_send_len
    syscall
    mov rax, 0
    mov rdi, 2
    syscall

.halt:
    jmp .halt

section .data
msg_start:     db 10, ">>> [USERSPACE NET] Demarrage test Sockets UDP (Ring 3)...", 10, 0
msg_start_len  equ $ - msg_start

msg_sock_ok:   db "[+] [USERSPACE NET] sys_socket() a reussi (Socket FD ouvert)!", 10, 0
msg_sock_ok_len equ $ - msg_sock_ok

msg_send_ok:   db "[+] [USERSPACE NET] sys_sendto() a reussi (Paquet UDP emis)! Termine.", 10, 0
msg_send_ok_len equ $ - msg_send_ok

err_sock:      db "[-] [USERSPACE NET] Echec sys_socket()!", 10, 0
err_sock_len   equ $ - err_sock

err_send:      db "[-] [USERSPACE NET] Echec sys_sendto()!", 10, 0
err_send_len   equ $ - err_send

payload:       db "Salam from YOS Ring 3 Network Stack!"
payload_len    equ $ - payload

align 4
dest_addr:
    dw 2                 ; sin_family = AF_INET (2)
    dw 0x0F27            ; sin_port = htons(9999)
    dd 0x0202000A        ; sin_addr = 10.0.2.2 (Little-endian: 10, 0, 2, 2)
    dq 0                 ; sin_zero[8]
