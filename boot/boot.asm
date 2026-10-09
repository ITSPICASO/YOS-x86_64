[bits 32]
global start
global pml4_table

section .boot
start:
    cli
    mov esp, stack_top

    mov edi, eax
    mov esi, ebx

    ; 1. Configuration des tables de pagination
    mov eax, pdpt_table
    or eax, 0b11
    mov [pml4_table], eax
    mov dword [pml4_table + 4], 0
    mov [pml4_table + 511 * 8], eax
    mov dword [pml4_table + 511 * 8 + 4], 0

    mov eax, pd_table
    or eax, 0b11
    mov [pdpt_table], eax
    mov dword [pdpt_table + 4], 0
    mov [pdpt_table + 510 * 8], eax
    mov dword [pdpt_table + 510 * 8 + 4], 0

    ; Remplir le PD (512 x 2MB Huge Pages = 1GB)
    mov ecx, 0
.loop_pd:
    mov eax, 0x200000
    mul ecx
    or eax, 0b10000011
    mov [pd_table + ecx * 8], eax
    mov dword [pd_table + ecx * 8 + 4], 0
    inc ecx
    cmp ecx, 512
    jne .loop_pd

    ; Charger CR3
    mov eax, pml4_table
    mov cr3, eax

    ; Activer PAE (CR4 bit 5)
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    ; Activer Long Mode (EFER MSR bit 8)
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    ; Activer Paging (CR0 bit 31)
    mov eax, cr0
    or eax, 0x80000001
    mov cr0, eax

    ; Charger GDT 64-bit temporaire
    lgdt [gdt64_ptr]
    jmp 0x08:long_mode_start

align 8
gdt64:
    dq 0
    dq 0x0020980000000000 ; CS 64-bit (0x08)
    dq 0x0000920000000000 ; DS 64-bit (0x10)
gdt64_ptr:
    dw $ - gdt64 - 1
    dd gdt64
    dd 0

[bits 64]
long_mode_start:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax

    ; Saut direct et absolu dans le Higher-Half (.text)
    mov rax, long_mode_entry
    jmp rax

section .boot_stack nobits alloc write
align 4096
pml4_table:
    resb 4096
pdpt_table:
    resb 4096
pd_table:
    resb 4096
stack_bottom:
    resb 16384
stack_top:

section .text
extern kmain
global long_mode_entry
long_mode_entry:
    ; Charger la pile 64-bit dans le Higher-Half
    mov rsp, stack_top + 0xFFFFFFFF80000000
    mov rdi, rsi
    call kmain
    cli
    hlt
