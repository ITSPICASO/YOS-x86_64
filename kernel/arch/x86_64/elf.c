#include "elf.h"
#include "serial.h"
#include "pmm.h"
#include "vmm.h"
#include "string.h"

bool elf_load(page_table_t *pml4, const void *elf_data, uint64_t *entry_point) {
    if (!elf_data || !pml4 || !entry_point) return false;

    const elf64_ehdr_t *ehdr = (const elf64_ehdr_t *)elf_data;

    /* Validation du Magic Number */
    if (ehdr->e_ident[0] != 0x7F || ehdr->e_ident[1] != 'E' ||
        ehdr->e_ident[2] != 'L'  || ehdr->e_ident[3] != 'F') {
        serial_print("[-] ELF: Magic incorrect!\n");
        return false;
    }

    /* ELF64 (Class 2) w Little-Endian (Data 1) */
    if (ehdr->e_ident[4] != 2 || ehdr->e_ident[5] != 1) {
        serial_print("[-] ELF: Format machi 64-bit Little-Endian!\n");
        return false;
    }

    /* Target x86_64 */
    if (ehdr->e_machine != EM_X86_64) {
        serial_print("[-] ELF: Machine machi x86_64!\n");
        return false;
    }

    serial_print("[+] ELF64 binaire valide. Entry point: 0x");
    serial_print_hex(ehdr->e_entry);
    serial_print("\n");

    const uint8_t *raw = (const uint8_t *)elf_data;
    const elf64_phdr_t *phdrs = (const elf64_phdr_t *)(raw + ehdr->e_phoff);

    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        const elf64_phdr_t *ph = &phdrs[i];

        if (ph->p_type != PT_LOAD) continue;

        /* User Space verification (< 0x0000800000000000) */
        if (ph->p_vaddr >= 0x0000800000000000ULL || (ph->p_vaddr + ph->p_memsz) >= 0x0000800000000000ULL) {
            serial_print("[-] ELF: Segment hors de l'espace utilisateur!\n");
            return false;
        }

        uint64_t vaddr_start = ph->p_vaddr;
        uint64_t vaddr_end   = ph->p_vaddr + ph->p_memsz;
        uint64_t page_start  = vaddr_start & ~0xFFFULL;
        uint64_t page_end    = (vaddr_end + 0xFFFULL) & ~0xFFFULL;

        /* Allocation w mapping dial les pages pour ce segment */
        for (uint64_t va = page_start; va < page_end; va += 4096) {
            void *phys = pmm_alloc_page();
            if (!phys) {
                serial_print("[-] ELF: Erreur allocation PMM!\n");
                return false;
            }

            /* Higher-half direct map bach n-nqqiw la page */
            uint64_t direct_map = (uint64_t)phys + 0xFFFFFFFF80000000ULL;
            memset((void *)direct_map, 0, 4096);

            /* Mapping f target PML4 b permissions User */
            vmm_map_page(pml4, va, (uint64_t)phys, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
        }

        /* Copie des données du fichier (filesz) vers vaddr */
        if (ph->p_filesz > 0) {
            memcpy((void *)ph->p_vaddr, raw + ph->p_offset, ph->p_filesz);
        }

        /* Nettoyage du BSS (memsz > filesz) */
        if (ph->p_memsz > ph->p_filesz) {
            memset((void *)(ph->p_vaddr + ph->p_filesz), 0, ph->p_memsz - ph->p_filesz);
        }

        serial_print("  -> PT_LOAD: [0x");
        serial_print_hex(ph->p_vaddr);
        serial_print(" - 0x");
        serial_print_hex(ph->p_vaddr + ph->p_memsz);
        serial_print("] charge avec succes.\n");
    }

    *entry_point = ehdr->e_entry;
    return true;
}
