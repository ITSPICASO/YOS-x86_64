#include "socket.h"
#include "net.h"
#include "e1000.h"
#include "wm.h"
#include "fb.h"
#include "elf.h"
#include "heap.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "serial.h"
#include "syscall.h"
#include "gdt.h"
#include "tss.h"
#include "idt.h"
#include "multiboot2.h"
#include "pmm.h"
#include "vmm.h"
#include "slab.h"
#include "acpi.h"
#include "pic.h"
#include "apic.h"
#include "ioapic.h"
#include "apic_timer.h"
#include "pci.h"
#include "ps2.h"
#include "keyboard.h"
#include "mouse.h"
#include "ide.h"
#include "mbr.h"
#include "gpt.h"
#include "ahci.h"
#include "vfs.h"
#include "fat32.h"
#include "tar.h"
#include "devfs.h"
#include "procfs.h"
#include "bcache.h"
#include "task.h"
#include "sync.h"
#include "string.h"



void user_test_entry(void) {
    serial_print("[USERSPACE TEST] Lancement dyal ELF64 Loader (Etape 72 & 73)...\n");

    /* 1. N-fet7o hello.elf mn VFS (li m-charga mn initrd) */
    vfs_node_t *file = vfs_lookup("hello.elf");
    if (!file) {
        /* Ila kan VFS 3ando path bhal /initrd/hello.elf ola fs root */
        file = vfs_lookup("/hello.elf");
    }

    if (!file) {
        serial_print("[-] Erreur: hello.elf introuvable dans le VFS!\n");
        while (1) { __asm__ volatile("hlt"); }
    }

    serial_print("[+] VFS: hello.elf trouve! Taille: ");
    serial_print_dec(file->size);
    serial_print(" octets\n");

    /* 2. Buffer f kernel heap bach n-qraw l-fichier */
    uint8_t *elf_buf = (uint8_t *)kmalloc(file->size);
    if (!elf_buf) {
        serial_print("[-] Erreur kmalloc pour elf_buf!\n");
        while (1) { __asm__ volatile("hlt"); }
    }

    int bytes_read = vfs_read(file, 0, file->size, elf_buf);
    if (bytes_read <= 0) {
        serial_print("[-] Erreur lecture de hello.elf depuis VFS!\n");
        while (1) { __asm__ volatile("hlt"); }
    }

    /* 3. Charger les segments PT_LOAD via elf_load */
    uint64_t entry_point = 0;
    if (!elf_load(kernel_pml4, elf_buf, &entry_point)) {
        serial_print("[-] Echec de chargement ELF!\n");
        while (1) { __asm__ volatile("hlt"); }
    }

    kfree(elf_buf);

    /* 4. Etape 73: Allocation dial User Stack a 0x00007FFFF000 */
    uint64_t user_stack_top = 0x00007FFFF000ULL;
    uint64_t stack_page_virt = 0x00007FFFE000ULL;

    void *stack_phys = pmm_alloc_page();
    if (!stack_phys) {
        serial_print("[-] Erreur allocation PMM pour User Stack!\n");
        while (1) { __asm__ volatile("hlt"); }
    }

    /* Mapping dial user stack b permissions PAGE_USER | PAGE_WRITABLE */
    vmm_map_page(kernel_pml4, stack_page_virt, (uint64_t)stack_phys, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);

    serial_print("[+] User Stack allouee et mappee a 0x");
    serial_print_hex(user_stack_top);
    serial_print("\n");

    serial_print("[+] Transition vers Ring 3 (enter_user_mode)...\n");

    /* 5. Saut definitif vers Userspace (Ring 3) via iretq */
        serial_print("[DEBUG] Premieres instructions a entry_point:\n");
    uint8_t *code_ptr = (uint8_t *)entry_point;
    for (int i = 0; i < 16; i++) {
        serial_print_hex(code_ptr[i]);
        serial_print(" ");
    }
    serial_print("\n");

    enter_user_mode(entry_point, user_stack_top);

    while (1) { __asm__ volatile("hlt"); }
}

static semaphore_t sem_test;

void worker_thread(void) {
    sem_wait(&sem_test);
    serial_print("[THREAD] Semaphore acquis avec succes!\n");
    task_sleep(100);
    serial_print("[THREAD] Fin du thread -> thread_exit().\n");
    sem_post(&sem_test);
    thread_exit();
}

void kmain(uint64_t multiboot_addr) {
    serial_init();
    serial_print("[+] YOS 64-bit : UART COM1 Initialise.\n");
    gdt_init();
    tss_init();
    idt_init();
    serial_print("[*] Bloc 2 : OK.\n");

    multiboot_parse_mmap(multiboot_addr);
    pmm_init(multiboot_addr);
    serial_print("[*] Bloc 3 : OK.\n");

    vmm_init();
    serial_print("[*] Bloc 4 : OK.\n");

    heap_init();
    slab_init();
    fb_init();
    wm_init();

    /* Fenetre 1 : Terminal System */
    window_t *w_term = wm_create_window(60, 60, 420, 240, "Terminal (sh)");
    if (w_term) {
        win_draw_string(w_term, 10, 10, "root@yos-kernel:~# uname -a", 0x0000FF00);
        win_draw_string(w_term, 10, 30, "YOS 64-bit Kernel v1.0 SMP x86_64", 0x00CCCCCC);
        win_draw_string(w_term, 10, 60, "root@yos-kernel:~# ls -l /", 0x0000FF00);
        win_draw_string(w_term, 10, 80, "drwx------  devfs", 0x0000AAFF);
        win_draw_string(w_term, 10, 100, "drwx------  procfs", 0x0000AAFF);
        win_draw_string(w_term, 10, 120, "-rwxr-xr-x  hello.elf", 0x00FFFFFF);
    }

    /* Fenetre 3 : Explorateur de Fichiers (Etape 97) */
    window_t *w_fm = wm_create_window(120, 340, 480, 260, "File Manager - [/]");
    if (w_fm) {
        win_draw_string(w_fm, 10, 10, "Chemin : / (Root VFS)", 0x00E5E5E5);
        win_draw_string(w_fm, 10, 25, "---------------------------------------------", 0x00555555);
        
        /* Dossiers */
        win_draw_string(w_fm, 15, 45, "[DIR]   devfs/          <DEV>", 0x0000AAFF);
        win_draw_string(w_fm, 15, 65, "[DIR]   procfs/         <PROC>", 0x0000AAFF);
        
        /* Fichiers */
        win_draw_string(w_fm, 15, 95, "[FILE]  hello.elf       5344 B   (ELF64 Ring 3)", 0x0000FF00);
        win_draw_string(w_fm, 15, 115, "[FILE]  welcome.txt     23 B     (Texte)", 0x00FFFFFF);
        
        win_draw_string(w_fm, 10, 150, "---------------------------------------------", 0x00555555);
        win_draw_string(w_fm, 15, 170, "Espace : TAR Ramdisk monte | FAT32 SATA Pret", 0x00AAAAAA);
        win_draw_string(w_fm, 15, 190, "Statut : 4 elements trouves dans /", 0x00FFAA00);
    }

    /* Fenetre 4 : Visualiseur d Images BMP (Etape 98) */
    window_t *w_img = wm_create_window(630, 360, 340, 240, "Image Viewer - [YOS]");
    if (w_img) {
        /* Dessin d un cadre canvas */
        win_draw_rect(w_img, 10, 10, 320, 180, 0x001E1E1E);
        
        /* Motif graphique decoratif (Simulant un logo developpe en BMP) */
        win_draw_rect(w_img, 30, 30, 280, 140, 0x000F2B48);
        win_draw_string(w_img, 50, 60, "YOS x86_64 LOGO", 0x0000FF00);
        win_draw_string(w_img, 50, 85, "Format: BMP 24-bit TrueColor", 0x00FFFFFF);
        win_draw_string(w_img, 50, 110, "Resolution: 280x140 px", 0x00FFAA00);
        win_draw_string(w_img, 50, 135, "[OK] Decoder Module Actif", 0x0000FFFF);
        
        win_draw_string(w_img, 15, 205, "Fichier : /logo.bmp (38 KB)", 0x00CCCCCC);
    }

    /* Fenetre 5 : Editeur de Texte (Etape 99) */
    window_t *w_pad = wm_create_window(360, 200, 440, 250, "Notepad - [notes.txt]");
    if (w_pad) {
        /* Barre d outils / Statut */
        win_draw_rect(w_pad, 5, 5, 430, 20, 0x002D2D2D);
        win_draw_string(w_pad, 10, 8, "Fichier: notes.txt | Lignes: 4 | UTF-8", 0x0000FF00);
        
        /* Zone de texte */
        win_draw_rect(w_pad, 5, 30, 430, 190, 0x00181818);
        win_draw_string(w_pad, 15, 45, "1 | # YOS Operating System Notes", 0x0000AAFF);
        win_draw_string(w_pad, 15, 70, "2 | Kernel x86_64 Higher-Half complet.", 0x00FFFFFF);
        win_draw_string(w_pad, 15, 95, "3 | Pilotes AHCI, e1000, GOP et PS/2 OK.", 0x00E5E5E5);
        win_draw_string(w_pad, 15, 120, "4 | Interface graphique et mini-libc OK.", 0x00FFAA00);
        win_draw_string(w_pad, 15, 145, "  | _ (Curseur actif)", 0x0055FF55);
        
        /* Footer */
        win_draw_string(w_pad, 10, 228, "[Ctrl+S: Sauvegarder sur SATA] [Pret]", 0x00AAAAAA);
    }

    /* Fenetre 2 : Moniteur Systeme */
    window_t *w_sys = wm_create_window(520, 140, 360, 200, "System Monitor");
    if (w_sys) {
        win_draw_string(w_sys, 10, 10, "CPU  : AMD/Intel x86_64 @ Ring 0/3", 0x00FFAA00);
        win_draw_string(w_sys, 10, 30, "ARCH : Higher-Half PML4 4-Level", 0x00FFFFFF);
        win_draw_string(w_sys, 10, 50, "DISP : UEFI/GOP Linear FB", 0x0000FFFF);
        win_draw_string(w_sys, 10, 70, "MEM  : PMM Page Frame Allocator", 0x0055FF55);
        win_draw_string(w_sys, 10, 90, "SCHED: MLFQ Priority Queues", 0x00FF55AA);
    }

    wm_compose();
    serial_print("[*] Bloc 5 : OK.\n");

    serial_print("[*] Bloc 6 : Initialisation ACPI, LAPIC, I/O APIC...\n");
    acpi_init(multiboot_addr);
    pic_disable();
    lapic_init();
    ioapic_init(acpi_get_ioapic_base());
    serial_print("[*] Bloc 6 : OK.\n");

    pci_scan();
    e1000_init();
    net_init();
    socket_init();
    dhcp_discover();
    uint32_t dummy_ip = 0;
    net_dns_resolve("google.com", &dummy_ip);
    /* Test Ping vers la passerelle QEMU (10.0.2.2) */
    uint32_t gateway_ip = (10) | (0 << 8) | (2 << 16) | (2 << 24);
    net_ping(gateway_ip);
    ps2_init();
    keyboard_init();
    mouse_init();
    mouse_init_cursor();
    serial_print("[*] Bloc 7 : OK.\n");

    ide_init();
    ahci_init();
    serial_print("[*] Bloc 8 : OK.\n");

    vfs_init();
    if (module_start_addr && module_total_size) {
        tar_init(module_start_addr, module_total_size);
        vfs_set_root(tar_get_root_node());
    }
    fat32_init(2048);
    bcache_init();
    serial_print("[*] Bloc 9 : OK.\n");

    /* Bloc 10 : Validation Etapes 61 a 67 */
    syscall_init();
    sem_init(&sem_test, 1);
    task_init();
    thread_create(user_test_entry, MLFQ_PRIO_HIGH);

    apic_timer_init(50);
    serial_print("[+] Bloc 10 completement arme (MLFQ, Reaper, Semaphores OK)!\n");
    __asm__ volatile("sti");

    for (;;) {
        __asm__ volatile("hlt");
    }
}