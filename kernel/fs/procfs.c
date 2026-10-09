#include "procfs.h"
#include "serial.h"
#include "pmm.h"
#include "heap.h"

#define PROCFS_MAX_NODES 8

static vfs_node_t proc_root;
static vfs_ops_t  proc_root_ops;

static vfs_node_t proc_nodes[PROCFS_MAX_NODES];
static vfs_ops_t  proc_ops[PROCFS_MAX_NODES];
static int        proc_count = 0;

static char meminfo_buf[512];
static char cpuinfo_buf[512];

static int uint_to_str(uint64_t val, char *buf) {
    char tmp[32];
    int i = 0, j = 0;
    if (val == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return 1;
    }
    while (val > 0) {
        tmp[i++] = (val % 10) + '0';
        val /= 10;
    }
    for (int k = i - 1; k >= 0; k--) {
        buf[j++] = tmp[k];
    }
    buf[j] = '\0';
    return j;
}

static int append_str(char *dest, const char *src, int pos) {
    while (*src) {
        dest[pos++] = *src++;
    }
    dest[pos] = '\0';
    return pos;
}

/* /proc/meminfo */
static int proc_meminfo_read(vfs_node_t *node, uint64_t offset, size_t size, uint8_t *buffer) {
    (void)node;
    uint64_t total_kb = pmm_get_total_memory() / 1024;
    uint64_t free_kb  = pmm_get_free_memory() / 1024;
    uint64_t used_kb  = pmm_get_used_memory() / 1024;

    int len = 0;
    char num[32];

    len = append_str(meminfo_buf, "MemTotal:     ", len);
    uint_to_str(total_kb, num);
    len = append_str(meminfo_buf, num, len);
    len = append_str(meminfo_buf, " kB\nMemFree:      ", len);

    uint_to_str(free_kb, num);
    len = append_str(meminfo_buf, num, len);
    len = append_str(meminfo_buf, " kB\nMemUsed:      ", len);

    uint_to_str(used_kb, num);
    len = append_str(meminfo_buf, num, len);
    len = append_str(meminfo_buf, " kB\n", len);

    if (offset >= (uint64_t)len) return 0;
    size_t to_copy = size;
    if (offset + to_copy > (uint64_t)len) to_copy = len - offset;

    for (size_t i = 0; i < to_copy; i++) {
        buffer[i] = meminfo_buf[offset + i];
    }
    return (int)to_copy;
}

/* /proc/cpuinfo */
static int proc_cpuinfo_read(vfs_node_t *node, uint64_t offset, size_t size, uint8_t *buffer) {
    (void)node;
    uint32_t eax, ebx, ecx, edx;
    char vendor[13];

    __asm__ volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0));
    *(uint32_t *)&vendor[0] = ebx;
    *(uint32_t *)&vendor[4] = edx;
    *(uint32_t *)&vendor[8] = ecx;
    vendor[12] = '\0';

    int len = 0;
    len = append_str(cpuinfo_buf, "processor:    0\nvendor_id:    ", len);
    len = append_str(cpuinfo_buf, vendor, len);
    len = append_str(cpuinfo_buf, "\narch:         x86_64 Long Mode\n", len);

    if (offset >= (uint64_t)len) return 0;
    size_t to_copy = size;
    if (offset + to_copy > (uint64_t)len) to_copy = len - offset;

    for (size_t i = 0; i < to_copy; i++) {
        buffer[i] = cpuinfo_buf[offset + i];
    }
    return (int)to_copy;
}

static vfs_node_t *procfs_finddir(vfs_node_t *node, const char *name) {
    (void)node;
    for (int i = 0; i < proc_count; i++) {
        const char *a = proc_nodes[i].name;
        const char *b = name;
        int match = 1;
        while (*a && *b) {
            if (*a != *b) { match = 0; break; }
            a++; b++;
        }
        if (match && *a == '\0' && *b == '\0') {
            return &proc_nodes[i];
        }
    }
    return NULL;
}

static void procfs_register_entry(const char *name, int (*read_fn)(vfs_node_t *, uint64_t, size_t, uint8_t *)) {
    if (proc_count >= PROCFS_MAX_NODES) return;

    int idx = proc_count++;
    int i = 0;
    for (; name[i] && i < 127; i++) proc_nodes[idx].name[i] = name[i];
    proc_nodes[idx].name[i] = '\0';
    proc_nodes[idx].flags = VFS_FILE;
    proc_nodes[idx].size = 256;
    proc_nodes[idx].device_ptr = NULL;

    proc_ops[idx].read = read_fn;
    proc_ops[idx].write = NULL;
    proc_ops[idx].finddir = NULL;
    proc_nodes[idx].ops = &proc_ops[idx];
}

void procfs_init(void) {
    serial_print("[*] Etape 59 : Initialisation de procfs (/proc)...\n");

    proc_root_ops.read = NULL;
    proc_root_ops.write = NULL;
    proc_root_ops.finddir = procfs_finddir;

    proc_root.name[0] = 'p';
    proc_root.name[1] = 'r';
    proc_root.name[2] = 'o';
    proc_root.name[3] = 'c';
    proc_root.name[4] = '\0';
    proc_root.flags = VFS_DIRECTORY;
    proc_root.size = 0;
    proc_root.ops = &proc_root_ops;

    procfs_register_entry("meminfo", proc_meminfo_read);
    procfs_register_entry("cpuinfo", proc_cpuinfo_read);

    serial_print("[+] procfs: /proc/meminfo et /proc/cpuinfo crees avec succes!\n");
}

vfs_node_t *procfs_get_root_node(void) {
    return &proc_root;
}
