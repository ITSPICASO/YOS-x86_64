#include "devfs.h"
#include "serial.h"
#include "heap.h"

#define DEVFS_MAX_NODES 8

static vfs_node_t  dev_root;
static vfs_ops_t   dev_root_ops;

static vfs_node_t  dev_nodes[DEVFS_MAX_NODES];
static vfs_ops_t   dev_ops[DEVFS_MAX_NODES];
static int         dev_count = 0;

/* /dev/null */
static int dev_null_read(vfs_node_t *node, uint64_t offset, size_t size, uint8_t *buffer) {
    (void)node; (void)offset; (void)size; (void)buffer;
    return 0; /* EOF immédiat */
}

static int dev_null_write(vfs_node_t *node, uint64_t offset, size_t size, const uint8_t *buffer) {
    (void)node; (void)offset; (void)buffer;
    return (int)size; /* Tout est absorbé */
}

/* /dev/zero */
static int dev_zero_read(vfs_node_t *node, uint64_t offset, size_t size, uint8_t *buffer) {
    (void)node; (void)offset;
    for (size_t i = 0; i < size; i++) {
        buffer[i] = 0;
    }
    return (int)size;
}

static int dev_zero_write(vfs_node_t *node, uint64_t offset, size_t size, const uint8_t *buffer) {
    (void)node; (void)offset; (void)buffer;
    return (int)size;
}

static vfs_node_t *devfs_finddir(vfs_node_t *node, const char *name) {
    (void)node;
    for (int i = 0; i < dev_count; i++) {
        const char *a = dev_nodes[i].name;
        const char *b = name;
        int match = 1;
        while (*a && *b) {
            if (*a != *b) { match = 0; break; }
            a++; b++;
        }
        if (match && *a == '\0' && *b == '\0') {
            return &dev_nodes[i];
        }
    }
    return NULL;
}

static void devfs_register_device(const char *name, 
                                  int (*read_fn)(vfs_node_t *, uint64_t, size_t, uint8_t *),
                                  int (*write_fn)(vfs_node_t *, uint64_t, size_t, const uint8_t *)) {
    if (dev_count >= DEVFS_MAX_NODES) return;

    int idx = dev_count++;
    int i = 0;
    for (; name[i] && i < 127; i++) dev_nodes[idx].name[i] = name[i];
    dev_nodes[idx].name[i] = '\0';
    dev_nodes[idx].flags = VFS_FILE;
    dev_nodes[idx].size = 0;
    dev_nodes[idx].device_ptr = NULL;

    dev_ops[idx].read = read_fn;
    dev_ops[idx].write = write_fn;
    dev_ops[idx].finddir = NULL;
    dev_nodes[idx].ops = &dev_ops[idx];
}

void devfs_init(void) {
    serial_print("[*] Etape 58 : Initialisation du systeme de fichiers de peripheriques devfs...\n");

    dev_root_ops.read = NULL;
    dev_root_ops.write = NULL;
    dev_root_ops.finddir = devfs_finddir;

    dev_root.name[0] = 'd';
    dev_root.name[1] = 'e';
    dev_root.name[2] = 'v';
    dev_root.name[3] = '\0';
    dev_root.flags = VFS_DIRECTORY;
    dev_root.size = 0;
    dev_root.ops = &dev_root_ops;

    devfs_register_device("null", dev_null_read, dev_null_write);
    devfs_register_device("zero", dev_zero_read, dev_zero_write);

    serial_print("[+] devfs: /dev/null et /dev/zero enregistres avec succes!\n");
}

vfs_node_t *devfs_get_root_node(void) {
    return &dev_root;
}
