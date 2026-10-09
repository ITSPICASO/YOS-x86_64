with open("kernel/arch/x86_64/syscall_entry.asm", "r") as f:
    code = f.read()

old_stack_load = """    lea rsp, [rel kernel_syscall_stack_top]"""

new_stack_load = """    ; Charger la pile noyau propre au thread courant (offset 32 = stack_base, + 4096)
    push rax
    mov rax, [rel current_thread]
    test rax, rax
    jz .use_global_stack
    mov rsp, [rax + 32]       ; stack_base (offset: rsp=0, user_rsp=8, tid=16, state=20, prio=24, slice=28, wake=32?)
    test rsp, rsp
    jz .use_global_stack
    add rsp, 4096             ; haut de pile
    jmp .stack_ready
.use_global_stack:
    lea rsp, [rel kernel_syscall_stack_top]
.stack_ready:
    pop rax"""

# Pour eviter les soucis d'offset exact de stack_base en ASM, utilisons une fonction C propre:
EOFcat << 'EOF' > patch_syscall_stack_per_thread.py
with open("kernel/arch/x86_64/syscall_entry.asm", "r") as f:
    code = f.read()

old_stack_load = """    lea rsp, [rel kernel_syscall_stack_top]"""

new_stack_load = """    ; Charger la pile noyau propre au thread courant (offset 32 = stack_base, + 4096)
    push rax
    mov rax, [rel current_thread]
    test rax, rax
    jz .use_global_stack
    mov rsp, [rax + 32]       ; stack_base (offset: rsp=0, user_rsp=8, tid=16, state=20, prio=24, slice=28, wake=32?)
    test rsp, rsp
    jz .use_global_stack
    add rsp, 4096             ; haut de pile
    jmp .stack_ready
.use_global_stack:
    lea rsp, [rel kernel_syscall_stack_top]
.stack_ready:
    pop rax"""

# Pour eviter les soucis d'offset exact de stack_base en ASM, utilisons une fonction C propre:
