import os
from pathlib import Path

files_to_collect = [
    # Boot & Linker
    "boot/multiboot_header.asm",
    "boot/boot.asm",
    "linker.ld",
    "Makefile",

    # Headers
    "kernel/include/gdt.h",
    "kernel/include/tss.h",
    "kernel/include/idt.h",
    "kernel/include/isr.h",
    "kernel/include/page_fault.h",
    "kernel/include/double_fault.h",
    "kernel/include/serial.h",
    "kernel/include/io.h",
    "kernel/include/multiboot2.h",
    "kernel/include/pmm.h",
    "kernel/include/vmm.h",
    "kernel/include/heap.h",
    "kernel/include/pic.h",
    "kernel/include/apic.h",
    "kernel/include/apic_timer.h",

    # Source files (ASM & C)
    "kernel/arch/x86_64/gdt_flush.asm",
    "kernel/arch/x86_64/gdt.c",
    "kernel/arch/x86_64/tss_flush.asm",
    "kernel/arch/x86_64/tss.c",
    "kernel/arch/x86_64/idt_flush.asm",
    "kernel/arch/x86_64/idt.c",
    "kernel/arch/x86_64/isr_stubs.asm",
    "kernel/arch/x86_64/isr.c",
    "kernel/arch/x86_64/page_fault.c",
    "kernel/arch/x86_64/double_fault.c",
    "kernel/arch/x86_64/serial.c",
    "kernel/arch/x86_64/multiboot2.c",
    "kernel/arch/x86_64/pmm.c",
    "kernel/arch/x86_64/vmm.c",
    "kernel/arch/x86_64/heap.c",
    "kernel/arch/x86_64/pic.c",
    "kernel/arch/x86_64/apic.c",
    "kernel/arch/x86_64/apic_timer_stub.asm",
    "kernel/arch/x86_64/apic_timer.c",
    "kernel/kernel.c"
]

output_filename = "yos_full_context.txt"

with open(output_filename, "w", encoding="utf-8") as out:
    for filepath in files_to_collect:
        path = Path(filepath)
        out.write(f"\n{'='*30}\n")
        out.write(f"FILE: {filepath}\n")
        out.write(f"{'='*30}\n\n")
        if path.exists():
            try:
                out.write(path.read_text(encoding="utf-8", errors="replace"))
            except Exception as e:
                out.write(f"ERROR READING FILE: {e}\n")
        else:
            out.write("FILE NOT FOUND (OPTIONAL OR MISSING)\n")
        out.write("\n")

print(f"[+] Succès: Fichier généré -> {output_filename}")
