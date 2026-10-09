[bits 64]
section .text
global mouse_stub
extern mouse_handler

mouse_stub:
    cld
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11

    test rsp, 15
    jz .aligned
    sub rsp, 8
    call mouse_handler
    add rsp, 8
    jmp .done

.aligned:
    call mouse_handler

.done:
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax
    iretq
