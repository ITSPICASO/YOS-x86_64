#include "vfs.h"
#include "serial.h"

#define MAX_MOUNTS 8

typedef struct {
    char        path[64];
    vfs_node_t *node;
} vfs_mount_t;

static vfs_node_t  *root_node = NULL;
static vfs_mount_t  mount_table[MAX_MOUNTS];
static int          mount_count = 0;

void vfs_init(void) {
    serial_print("[*] Initialisation du Virtual File System (VFS)...\n");
    root_node = NULL;
    mount_count = 0;
}

void vfs_set_root(vfs_node_t *node) {
    root_node = node;
    serial_print("[+] VFS: Racine '/' montee avec succes.\n");
}

vfs_node_t *vfs_get_root(void) {
    return root_node;
}

void vfs_mount(const char *mount_point, vfs_node_t *fs_root) {
    if (mount_count < MAX_MOUNTS) {
        int i = 0;
        for (; mount_point[i] && i < 63; i++) {
            mount_table[mount_count].path[i] = mount_point[i];
        }
        mount_table[mount_count].path[i] = '\0';
        mount_table[mount_count].node = fs_root;
        mount_count++;
        serial_print("[+] VFS: Point de montage '");
        serial_print(mount_point);
        serial_print("' active.\n");
    }
}

int vfs_read(vfs_node_t *node, uint64_t offset, size_t size, uint8_t *buffer) {
    if (node && node->ops && node->ops->read) {
        return node->ops->read(node, offset, size, buffer);
    }
    return -1;
}

int vfs_write(vfs_node_t *node, uint64_t offset, size_t size, const uint8_t *buffer) {
    if (node && node->ops && node->ops->write) {
        return node->ops->write(node, offset, size, buffer);
    }
    return -1;
}

static bool str_starts_with(const char *str, const char *prefix) {
    while (*prefix) {
        if (*prefix != *str) return false;
        prefix++;
        str++;
    }
    return true;
}

vfs_node_t *vfs_lookup(const char *path) {
    if (!root_node) return NULL;
    if (path[0] == '/' && path[1] == '\0') return root_node;

    /* Vérifier si le chemin commence par un point de montage */
    for (int m = 0; m < mount_count; m++) {
        if (str_starts_with(path, mount_table[m].path)) {
            const char *sub_path = path + 0;
            int len = 0;
            while (mount_table[m].path[len]) len++;
            sub_path = path + len;
            if (*sub_path == '/') sub_path++;

            if (*sub_path == '\0') return mount_table[m].node;
            if (mount_table[m].node->ops && mount_table[m].node->ops->finddir) {
                return mount_table[m].node->ops->finddir(mount_table[m].node, sub_path);
            }
        }
    }

    const char *p = (path[0] == '/') ? path + 1 : path;
    char token[128];
    vfs_node_t *curr = root_node;

    while (*p) {
        int idx = 0;
        while (*p && *p != '/' && idx < 127) {
            token[idx++] = *p++;
        }
        token[idx] = '\0';
        if (*p == '/') p++;

        if (curr->ops && curr->ops->finddir) {
            curr = curr->ops->finddir(curr, token);
            if (!curr) return NULL;
        } else {
            return NULL;
        }
    }

    return curr;
}
