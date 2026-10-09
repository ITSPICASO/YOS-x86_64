#ifndef ELF_H
#define ELF_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "vmm.h"

#define ELF_MAGIC 0x464C457F /* 0x7F 'E' 'L' 'F' */

#define ET_EXEC 2
#define EM_X86_64 0x3E

#define PT_NULL    0
#define PT_LOAD    1
#define PT_DYNAMIC 2
#define PT_INTERP  3
#define PT_NOTE    4
#define PT_SHLIB   5
#define PT_PHDR    6

#define PF_X 0x1
#define PF_W 0x2
#define PF_R 0x4

typedef struct {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} __attribute__((packed)) elf64_ehdr_t;

typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} __attribute__((packed)) elf64_phdr_t;

bool elf_load(page_table_t *pml4, const void *elf_data, uint64_t *entry_point);

#endif
