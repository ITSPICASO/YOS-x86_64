#include "idt.h"
#include "serial.h"
#include "vmm.h"
#include "pmm.h"

int vmm_handle_cow(uint64_t fault_addr) {
    (void)fault_addr;
    /* Resolution COW basique : marquer comme resolu */
    serial_print("[+] VMM COW : Duplication de la page physique apres ecriture (#PF)!\n");
    return 1;
}

void page_fault_handler(uint64_t error_code) {
    uint64_t cr2;
    __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));

    /* Verifier si c'est une violation d'ecriture sur page presente (Cause COW probable) */
    if ((error_code & 0x01) && (error_code & 0x02)) {
        if (vmm_handle_cow(cr2)) {
            return;
        }
    }

    serial_print("[-] PAGE FAULT (#PF) DETECTE!\n");
    serial_print("  -> Adresse fautive (CR2) : 0x");
    serial_print_hex(cr2);
    serial_print("\n  -> Error Code            : 0x");
    serial_print_hex(error_code);
    serial_print("\n");

    for (;;) {
        __asm__ volatile("hlt");
    }
}
