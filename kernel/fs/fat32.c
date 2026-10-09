#include "fat32.h"
#include "ahci.h"
#include "serial.h"
#include "heap.h"

static uint32_t partition_lba = 0;
static uint32_t fat_start_lba = 0;
static uint32_t data_start_lba = 0;
static uint8_t  sectors_per_cluster = 0;
static uint32_t root_dir_cluster = 0;
static uint32_t total_clusters = 0;

static vfs_node_t fat_root;
static vfs_ops_t  fat_ops;

static inline uint32_t cluster_to_lba(uint32_t cluster) {
    return data_start_lba + ((cluster - 2) * sectors_per_cluster);
}

static uint32_t fat32_get_next_cluster(uint32_t cluster) {
    uint32_t fat_sector = fat_start_lba + ((cluster * 4) / 512);
    uint32_t fat_offset = (cluster * 4) % 512;

    uint8_t sec_buf[512] __attribute__((aligned(4096)));
    if (!ahci_read_sectors(0, fat_sector, 1, sec_buf)) return 0x0FFFFFF8;

    uint32_t next = *(uint32_t *)&sec_buf[fat_offset];
    return next & 0x0FFFFFFF;
}

static bool fat32_set_next_cluster(uint32_t cluster, uint32_t next_value) {
    uint32_t fat_sector = fat_start_lba + ((cluster * 4) / 512);
    uint32_t fat_offset = (cluster * 4) % 512;

    uint8_t sec_buf[512] __attribute__((aligned(4096)));
    if (!ahci_read_sectors(0, fat_sector, 1, sec_buf)) return false;

    uint32_t *entry = (uint32_t *)&sec_buf[fat_offset];
    *entry = (*entry & 0xF0000000) | (next_value & 0x0FFFFFFF);

    return ahci_write_sectors(0, fat_sector, 1, sec_buf);
}

static uint32_t fat32_allocate_cluster(uint32_t last_cluster) {
    uint8_t sec_buf[512] __attribute__((aligned(4096)));

    for (uint32_t cl = 2; cl < total_clusters; cl++) {
        uint32_t fat_sector = fat_start_lba + ((cl * 4) / 512);
        uint32_t fat_offset = (cl * 4) % 512;

        if (!ahci_read_sectors(0, fat_sector, 1, sec_buf)) return 0;
        uint32_t val = (*(uint32_t *)&sec_buf[fat_offset]) & 0x0FFFFFFF;

        if (val == 0) {
            fat32_set_next_cluster(cl, 0x0FFFFFFF);
            if (last_cluster >= 2) {
                fat32_set_next_cluster(last_cluster, cl);
            }
            return cl;
        }
    }
    return 0;
}

static int fat32_read(vfs_node_t *node, uint64_t offset, size_t size, uint8_t *buffer) {
    uint32_t cluster = (uint32_t)(uint64_t)node->device_ptr;
    uint32_t cluster_size = sectors_per_cluster * 512;

    while (offset >= cluster_size && cluster < 0x0FFFFFF8) {
        cluster = fat32_get_next_cluster(cluster);
        offset -= cluster_size;
    }

    size_t bytes_read = 0;
    uint8_t cluster_buf[4096] __attribute__((aligned(4096)));

    while (bytes_read < size && cluster < 0x0FFFFFF8) {
        uint32_t lba = cluster_to_lba(cluster);
        if (!ahci_read_sectors(0, lba, sectors_per_cluster, cluster_buf)) break;

        size_t to_copy = cluster_size - offset;
        if (to_copy > (size - bytes_read)) to_copy = size - bytes_read;

        for (size_t i = 0; i < to_copy; i++) {
            buffer[bytes_read + i] = cluster_buf[offset + i];
        }

        bytes_read += to_copy;
        offset = 0;
        cluster = fat32_get_next_cluster(cluster);
    }

    return (int)bytes_read;
}

int fat32_write(vfs_node_t *node, uint64_t offset, size_t size, const uint8_t *buffer) {
    uint32_t cluster = (uint32_t)(uint64_t)node->device_ptr;
    uint32_t cluster_size = sectors_per_cluster * 512;

    while (offset >= cluster_size) {
        uint32_t next = fat32_get_next_cluster(cluster);
        if (next >= 0x0FFFFFF8) {
            next = fat32_allocate_cluster(cluster);
            if (!next) return -1;
        }
        cluster = next;
        offset -= cluster_size;
    }

    size_t bytes_written = 0;
    uint8_t cluster_buf[4096] __attribute__((aligned(4096)));

    while (bytes_written < size) {
        uint32_t lba = cluster_to_lba(cluster);

        if (offset > 0 || (size - bytes_written) < cluster_size) {
            ahci_read_sectors(0, lba, sectors_per_cluster, cluster_buf);
        }

        size_t to_copy = cluster_size - offset;
        if (to_copy > (size - bytes_written)) to_copy = size - bytes_written;

        for (size_t i = 0; i < to_copy; i++) {
            cluster_buf[offset + i] = buffer[bytes_written + i];
        }

        if (!ahci_write_sectors(0, lba, sectors_per_cluster, cluster_buf)) break;

        bytes_written += to_copy;
        offset = 0;

        if (bytes_written < size) {
            uint32_t next = fat32_get_next_cluster(cluster);
            if (next >= 0x0FFFFFF8) {
                next = fat32_allocate_cluster(cluster);
                if (!next) break;
            }
            cluster = next;
        }
    }

    if (node->size < (uint32_t)(offset + bytes_written)) {
        node->size = (uint32_t)(offset + bytes_written);
    }

    return (int)bytes_written;
}

static vfs_node_t *fat32_finddir(vfs_node_t *node, const char *name) {
    uint32_t cluster = (uint32_t)(uint64_t)node->device_ptr;
    uint8_t sec_buf[4096] __attribute__((aligned(4096)));

    while (cluster < 0x0FFFFFF8) {
        uint32_t lba = cluster_to_lba(cluster);
        if (!ahci_read_sectors(0, lba, sectors_per_cluster, sec_buf)) return NULL;

        struct fat32_dirent *entries = (struct fat32_dirent *)sec_buf;
        int max_entries = (sectors_per_cluster * 512) / sizeof(struct fat32_dirent);

        for (int i = 0; i < max_entries; i++) {
            if ((uint8_t)entries[i].name[0] == 0x00) return NULL;
            if ((uint8_t)entries[i].name[0] == 0xE5) continue;
            if (entries[i].attr == 0x0F) continue;

            char fname[13];
            int pos = 0;
            for (int k = 0; k < 8 && entries[i].name[k] != ' '; k++) {
                fname[pos++] = entries[i].name[k];
            }
            if (entries[i].name[8] != ' ') {
                fname[pos++] = '.';
                for (int k = 8; k < 11 && entries[i].name[k] != ' '; k++) {
                    fname[pos++] = entries[i].name[k];
                }
            }
            fname[pos] = '\0';

            int match = 1;
            for (int k = 0; fname[k] || name[k]; k++) {
                char c1 = (fname[k] >= 'a' && fname[k] <= 'z') ? fname[k] - 32 : fname[k];
                char c2 = (name[k] >= 'a' && name[k] <= 'z') ? name[k] - 32 : name[k];
                if (c1 != c2) { match = 0; break; }
            }

            if (match) {
                vfs_node_t *found = (vfs_node_t *)kmalloc(sizeof(vfs_node_t));
                for (int k = 0; k < pos; k++) found->name[k] = fname[k];
                found->name[pos] = '\0';
                found->flags = (entries[i].attr & 0x10) ? VFS_DIRECTORY : VFS_FILE;
                found->size = entries[i].file_size;
                uint32_t start_cl = ((uint32_t)entries[i].cluster_high << 16) | entries[i].cluster_low;
                found->device_ptr = (void *)(uint64_t)start_cl;
                found->ops = &fat_ops;
                return found;
            }
        }
        cluster = fat32_get_next_cluster(cluster);
    }
    return NULL;
}

void fat32_init(uint32_t lba_start) {
    serial_print("[*] Montage du systeme de fichiers FAT32...\n");
    partition_lba = lba_start;

    uint8_t bpb_buf[512] __attribute__((aligned(4096)));
    if (!ahci_read_sectors(0, partition_lba, 1, bpb_buf)) {
        serial_print("[-] FAT32: Impossible de lire le secteur BPB!\n");
        return;
    }

    struct fat32_bpb *bpb = (struct fat32_bpb *)bpb_buf;
    sectors_per_cluster = bpb->sectors_per_cluster;
    if (sectors_per_cluster == 0) {
        serial_print("[-] FAT32: Secteur BPB non-FAT32 ou corrompu (sectors_per_cluster = 0).\n");
        return;
    }
    if (sectors_per_cluster == 0) {
        serial_print("[-] FAT32: Secteur BPB non-FAT32 ou corrompu (sectors_per_cluster = 0).\n");
        return;
    }
    fat_start_lba = partition_lba + bpb->reserved_sectors;
    uint32_t fat_sectors = bpb->fat_size_32 * bpb->num_fats;
    data_start_lba = fat_start_lba + fat_sectors;
    root_dir_cluster = bpb->root_cluster;
    total_clusters = (bpb->total_sectors_32 - (data_start_lba - partition_lba)) / sectors_per_cluster;

    serial_print("[+] FAT32: Detecte avec succes!\n");
    serial_print("  -> Secteurs par cluster : ");
    serial_print_dec(sectors_per_cluster);
    serial_print("\n  -> LBA Debut FAT        : ");
    serial_print_dec(fat_start_lba);
    serial_print("\n  -> LBA Debut Donnees    : ");
    serial_print_dec(data_start_lba);
    serial_print("\n  -> Cluster Racine       : ");
    serial_print_dec(root_dir_cluster);
    serial_print("\n");

    fat_ops.read = fat32_read;
    fat_ops.write = fat32_write;
    fat_ops.finddir = fat32_finddir;

    fat_root.name[0] = '/';
    fat_root.name[1] = '\0';
    fat_root.flags = VFS_DIRECTORY;
    fat_root.size = 0;
    fat_root.device_ptr = (void *)(uint64_t)root_dir_cluster;
    fat_root.ops = &fat_ops;

    vfs_set_root(&fat_root);
}
