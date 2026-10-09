#ifndef VFS_H
#define VFS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define VFS_FILE      0x01
#define VFS_DIRECTORY 0x02

struct vfs_node;

typedef struct {
    int (*read)(struct vfs_node *node, uint64_t offset, size_t size, uint8_t *buffer);
    int (*write)(struct vfs_node *node, uint64_t offset, size_t size, const uint8_t *buffer);
    struct vfs_node *(*finddir)(struct vfs_node *node, const char *name);
} vfs_ops_t;

typedef struct vfs_node {
    char        name[128];
    uint32_t    flags;
    uint32_t    size;
    uint32_t    inode;
    vfs_ops_t  *ops;
    void       *device_ptr;
} vfs_node_t;

void vfs_init(void);
void vfs_set_root(vfs_node_t *node);
vfs_node_t *vfs_get_root(void);
void vfs_mount(const char *mount_point, vfs_node_t *fs_root);
vfs_node_t *vfs_lookup(const char *path);
int vfs_read(vfs_node_t *node, uint64_t offset, size_t size, uint8_t *buffer);
int vfs_write(vfs_node_t *node, uint64_t offset, size_t size, const uint8_t *buffer);

#endif
