#!/bin/bash
OUTPUT="debug_dump.txt"
rm -f "$OUTPUT"

files=(
    "kernel/arch/x86_64/gdt.c"
    "kernel/include/gdt.h"
    "kernel/arch/x86_64/tss.c"
    "kernel/include/tss.h"
    "kernel/arch/x86_64/idt.c"
    "kernel/include/idt.h"
    "kernel/arch/x86_64/idt_flush.asm"
    "kernel/arch/x86_64/isr_stubs.asm"
    "kernel/arch/x86_64/apic_timer.c"
    "kernel/arch/x86_64/apic_timer_stub.asm"
    "boot/boot.asm"
)

for f in "${files[@]}"; do
    echo -e "\n==================== $f ====================" >> "$OUTPUT"
    if [ -f "$f" ]; then
        cat "$f" >> "$OUTPUT"
    else
        echo "[!] FICHIER INTROUVABLE: $f" >> "$OUTPUT"
    fi
done

echo "[+] Kolchi t-jme3 b naja7 f $OUTPUT"
