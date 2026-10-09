#ifndef PROCFS_H
#define PROCFS_H

#include "vfs.h"

void procfs_init(void);
vfs_node_t *procfs_get_root_node(void);

#endif
