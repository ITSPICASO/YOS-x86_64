#include "socket.h"
#include "heap.h"
#include "elf.h"
#include "vfs.h"
#include "pmm.h"
#include "vfs.h"
#include "string.h"
#include "syscall.h"
#include "serial.h"
#include "task.h"
#include "vmm.h"
#include "keyboard.h"
#include <stddef.h>
#include <stdbool.h>

/* MAX_SYSCALLS defini dans syscall.h (32) */

typedef int64_t (*syscall_fn_t)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
static syscall_fn_t syscall_table[MAX_SYSCALLS];

extern void syscall_entry(void);

#define IA32_EFER   0xC0000080
#define IA32_STAR   0xC0000081
#define IA32_LSTAR  0xC0000082
#define IA32_SFMASK 0xC0000084

static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t low, high;
    __asm__ volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

static inline void wrmsr(uint32_t msr, uint64_t val) {
    uint32_t low  = (uint32_t)(val & 0xFFFFFFFF);
    uint32_t high = (uint32_t)(val >> 32);
    __asm__ volatile("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}

bool copy_from_user(void *dst, const void *src, size_t n) {
    uint64_t addr = (uint64_t)src;
    if (addr == 0 || addr >= 0x800000000000ULL || (addr + n) > 0x800000000000ULL) {
        return false;
    }
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    for (size_t i = 0; i < n; i++) d[i] = s[i];
    return true;
}

bool copy_to_user(void *dst, const void *src, size_t n) {
    uint64_t addr = (uint64_t)dst;
    if (addr == 0 || addr >= 0x800000000000ULL || (addr + n) > 0x800000000000ULL) {
        return false;
    }
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    for (size_t i = 0; i < n; i++) d[i] = s[i];
    return true;
}

static int64_t sys_exit_handler(uint64_t status, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
    (void)a2; (void)a3; (void)a4; (void)a5;
    serial_print("[SYSCALL] sys_exit call with code: ");
    serial_print_dec(status);
    serial_print("\n");
    thread_exit();
    return 0;
}


#define MAX_FDS 32

typedef struct {
    vfs_node_t *node;
    uint64_t offset;
    bool in_use;
} file_desc_t;

static file_desc_t fd_table[MAX_FDS];

static int64_t sys_read_handler(uint64_t fd, uint64_t buf, uint64_t count, uint64_t a4, uint64_t a5) {
    (void)a4; (void)a5;
    if (fd == 0) { /* Stdin / Clavier PS/2 */
        if (count == 0) return 0;
        char kbuf[128];
        size_t read_bytes = 0;

        while (read_bytes < count) {
            while (!keyboard_has_char()) {
                __asm__ volatile("sti; hlt");
            }
            char c = keyboard_getchar();
            if (c == 0) continue;

            if (c == '\b' || c == 0x7F) {
                if (read_bytes > 0) {
                    read_bytes--;
                    serial_print("\b \b");
                }
                continue;
            }

            if (c == '\r') c = '\n';
            serial_putc(c);

            kbuf[read_bytes++] = c;
            if (c == '\n' || read_bytes >= sizeof(kbuf)) {
                break;
            }
        }

        if (!copy_to_user((void *)buf, kbuf, read_bytes)) {
            return -1;
        }
        return (int64_t)read_bytes;
    } else if (fd >= 3 && fd < MAX_FDS) {
        if (!fd_table[fd].in_use || !fd_table[fd].node) return -1;
        if (count == 0) return 0;

        char kbuf[512];
        size_t to_read = (count > sizeof(kbuf)) ? sizeof(kbuf) : count;
        int bytes = vfs_read(fd_table[fd].node, fd_table[fd].offset, to_read, (uint8_t *)kbuf);
        if (bytes <= 0) return bytes;

        if (!copy_to_user((void *)buf, kbuf, bytes)) {
            return -1;
        }

        fd_table[fd].offset += bytes;
        return (int64_t)bytes;
    }
    return -1;
}

static int64_t sys_open_handler(uint64_t path_ptr, uint64_t flags, uint64_t a3, uint64_t a4, uint64_t a5) {
    (void)flags; (void)a3; (void)a4; (void)a5;
    char path[128];
    if (!copy_from_user(path, (const void *)path_ptr, sizeof(path))) {
        return -1;
    }
    path[sizeof(path) - 1] = '\0';

    vfs_node_t *node = vfs_lookup(path);
    if (!node) {
        serial_print("[-] sys_open: Fichier non trouve: ");
        serial_print(path);
        serial_print("\n");
        return -1;
    }

    /* Trouver un descripteur libre a partir de 3 */
    for (int i = 3; i < MAX_FDS; i++) {
        if (!fd_table[i].in_use) {
            fd_table[i].in_use = true;
            fd_table[i].node = node;
            fd_table[i].offset = 0;
            return i;
        }
    }
    return -1;
}

static int64_t sys_close_handler(uint64_t fd, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
    (void)a2; (void)a3; (void)a4; (void)a5;
    if (fd >= 3 && fd < MAX_FDS && fd_table[fd].in_use) {
        fd_table[fd].in_use = false;
        fd_table[fd].node = NULL;
        fd_table[fd].offset = 0;
        return 0;
    }
    return -1;
}

static int64_t sys_write_handler(uint64_t fd, uint64_t buf, uint64_t len, uint64_t a4, uint64_t a5) {
    (void)a4; (void)a5;
    if (fd == 1 || fd == 2) {
        char kbuf[128];
        size_t rem = len;
        size_t off = 0;
        while (rem > 0) {
            size_t chunk = (rem > 127) ? 127 : rem;
            if (!copy_from_user(kbuf, (const void *)(buf + off), chunk)) {
                return -1;
            }
            kbuf[chunk] = '\0';
            serial_print(kbuf);
            rem -= chunk;
            off += chunk;
        }
        return (int64_t)len;
    }
    return -1;
}

static int64_t sys_yield_handler(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    task_yield();
    return 0;
}

int64_t syscall_dispatcher(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
    // serial_print("[SYSCALL] Appel ID: "); serial_print_dec(num); serial_print("\n");
    // serial_print("[SYSCALL] Appel ID: "); serial_print_dec(num); serial_print("\n");
    if (num >= MAX_SYSCALLS || !syscall_table[num]) {
        serial_print("[-] Syscall invalid: ");
        serial_print_dec(num);
        serial_print("\n");
        return -1;
    }
    return syscall_table[num](a1, a2, a3, a4, a5);
}


extern uint64_t user_rsp_save;
extern uint64_t user_rip_save;

static int64_t sys_fork_handler(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    return task_fork(user_rsp_save, user_rip_save);
}

static int64_t sys_waitpid_handler(uint64_t pid, uint64_t status_ptr, uint64_t options, uint64_t a4, uint64_t a5) {
    (void)options; (void)a4; (void)a5;
    int status = 0;
    int64_t res = task_waitpid((int32_t)pid, &status);
    if (status_ptr != 0) {
        copy_to_user((void *)status_ptr, &status, sizeof(int));
    }
    return res;
}


#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

typedef struct {
    uint32_t size;
    uint32_t flags;
    uint32_t inode;
} stat_t;

static int64_t sys_seek_handler(uint64_t fd, uint64_t offset, uint64_t whence, uint64_t a4, uint64_t a5) {
    (void)a4; (void)a5;
    if (fd < 3 || fd >= MAX_FDS || !fd_table[fd].in_use || !fd_table[fd].node) return -1;

    int64_t new_offset = fd_table[fd].offset;
    if (whence == SEEK_SET) {
        new_offset = (int64_t)offset;
    } else if (whence == SEEK_CUR) {
        new_offset += (int64_t)offset;
    } else if (whence == SEEK_END) {
        new_offset = (int64_t)fd_table[fd].node->size + (int64_t)offset;
    } else {
        return -1;
    }

    if (new_offset < 0) return -1;
    fd_table[fd].offset = (uint64_t)new_offset;
    return new_offset;
}

static int64_t sys_stat_handler(uint64_t path_ptr, uint64_t stat_buf_ptr, uint64_t a3, uint64_t a4, uint64_t a5) {
    (void)a3; (void)a4; (void)a5;
    char path[128];
    if (!copy_from_user(path, (const void *)path_ptr, sizeof(path))) return -1;
    path[sizeof(path) - 1] = '\0';

    vfs_node_t *node = vfs_lookup(path);
    if (!node) return -1;

    stat_t st;
    st.size = node->size;
    st.flags = node->flags;
    st.inode = node->inode;

    if (!copy_to_user((void *)stat_buf_ptr, &st, sizeof(stat_t))) return -1;
    return 0;
}

static int64_t sys_execve_handler(uint64_t path_ptr, uint64_t argv_ptr, uint64_t envp_ptr, uint64_t a4, uint64_t a5) {
    (void)argv_ptr; (void)envp_ptr; (void)a4; (void)a5;
    if (!current_thread || !current_thread->process) return -1;

    char path[128];
    if (!copy_from_user(path, (const void *)path_ptr, sizeof(path))) return -1;
    path[sizeof(path) - 1] = '\0';

    vfs_node_t *node = vfs_lookup(path);
    if (!node || node->size == 0) {
        serial_print("[-] execve: Fichier introuvable: ");
        serial_print(path);
        serial_print("\n");
        return -1;
    }

    uint8_t *elf_buf = (uint8_t *)kmalloc(node->size);
    if (!elf_buf) return -1;

    if (vfs_read(node, 0, node->size, elf_buf) <= 0) {
        kfree(elf_buf);
        return -1;
    }

    /* Charger ELF dans le CR3 courant */
    uint64_t entry = 0;
    page_table_t *pml4 = (page_table_t *)current_thread->process->cr3;
    if (!elf_load(pml4, elf_buf, &entry)) {
        serial_print("[-] execve: Format ELF non valide!\n");
        kfree(elf_buf);
        return -1;
    }
    kfree(elf_buf);

    /* Etape 73: Reinitialiser la User Stack et preparer argc/argv */
    uint64_t user_stack_top = 0x7FFFF000ULL + 4096ULL;
    /* Aligner sur 16 octets */
    user_stack_top &= ~0xF;

    /* Mettre a jour le contexte du thread pour sysret */
    current_thread->user_rsp = user_stack_top;

    /* Modifier le frame de retour dans la pile noyau pour sysret (RCX = entry_point) */
    /* Dans syscall_entry, RCX est restaure depuis [rsp + 8] avant sysret */
    /* On modifie directement user_rip_save si fallback ou via le frame */
    extern uint64_t user_rip_save;
    user_rip_save = entry;

    /* En SysV ABI, rax de retour doit etre entry point ou le handler redirige */
    serial_print("[+] execve: Programme charge avec succes. Entry: 0x");
    serial_print_hex(entry);
    serial_print("\n");

    /* Mise a jour du RIP pour le retour via syscall_entry */
    extern uint64_t user_rip_save;
    user_rip_save = entry;

    return 0;
}


/* Syscall 20: socket(domain, type, protocol) */
static int64_t sys_socket_handler(uint64_t domain, uint64_t type, uint64_t proto, uint64_t a4, uint64_t a5) {
    (void)a4; (void)a5;
    return sys_socket((int)domain, (int)type, (int)proto);
}

/* Syscall 21: sendto(sockfd, buf, len, flags, dest_addr, addrlen) */
static int64_t sys_sendto_handler(uint64_t sockfd, uint64_t u_buf, uint64_t len, uint64_t flags, uint64_t u_addr) {
    if (len > 1500) return -1;
    uint8_t kbuf[1500];
    if (!copy_from_user(kbuf, (const void *)u_buf, len)) return -1;

    struct sockaddr_in kaddr;
    if (!copy_from_user(&kaddr, (const void *)u_addr, sizeof(struct sockaddr_in))) return -1;

    return sys_sendto((int)sockfd, kbuf, (size_t)len, (int)flags, &kaddr);
}

/* Syscall 22: recvfrom(sockfd, buf, len, flags, src_addr, addrlen) */
static int64_t sys_recvfrom_handler(uint64_t sockfd, uint64_t u_buf, uint64_t len, uint64_t flags, uint64_t u_addr) {
    if (len > 1500) len = 1500;
    uint8_t kbuf[1500];
    struct sockaddr_in kaddr;

    long ret = sys_recvfrom((int)sockfd, kbuf, (size_t)len, (int)flags, &kaddr);
    if (ret > 0) {
        if (!copy_to_user((void *)u_buf, kbuf, (size_t)ret)) return -1;
        if (u_addr) {
            copy_to_user((void *)u_addr, &kaddr, sizeof(struct sockaddr_in));
        }
    }
    return ret;
}

void syscall_init(void) {
    serial_print("[*] Etape 68 : Configuration syscall/sysret MSRs...\n");

    uint64_t efer = rdmsr(IA32_EFER);
    wrmsr(IA32_EFER, efer | EFER_SCE);

    uint64_t star = ((uint64_t)0x0010ULL << 48) | ((uint64_t)0x0008ULL << 32);
    wrmsr(IA32_STAR, star);

    wrmsr(IA32_LSTAR, (uint64_t)syscall_entry);
    wrmsr(IA32_SFMASK, 0x200);

    for (int i = 0; i < MAX_SYSCALLS; i++) syscall_table[i] = NULL;
    syscall_table[SYS_EXIT]  = sys_exit_handler;
    syscall_table[SYS_READ]  = sys_read_handler;
    syscall_table[SYS_WRITE] = sys_write_handler;
    syscall_table[SYS_OPEN]  = sys_open_handler;
    syscall_table[SYS_CLOSE]   = sys_close_handler;
        syscall_table[SYS_FORK]    = sys_fork_handler;
    syscall_table[SYS_SOCKET]  = sys_socket_handler;
    syscall_table[SYS_SENDTO]  = sys_sendto_handler;
    syscall_table[SYS_RECVFROM] = sys_recvfrom_handler;
    syscall_table[SYS_WAITPID] = sys_waitpid_handler;
    syscall_table[SYS_EXECVE]  = sys_execve_handler;
    syscall_table[SYS_SEEK]    = sys_seek_handler;
    syscall_table[SYS_STAT]    = sys_stat_handler;

    for (int i = 0; i < MAX_FDS; i++) fd_table[i].in_use = false;
    syscall_table[SYS_YIELD] = sys_yield_handler;

    serial_print("[+] MSRs configures & Syscall Table prete.\n");
}
