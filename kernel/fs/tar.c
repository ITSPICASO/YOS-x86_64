#include "tar.h"
#include "serial.h"
#include "heap.h"

static uint64_t archive_start = 0;
static size_t   archive_len = 0;

static vfs_node_t tar_root;
static vfs_ops_t  tar_ops;

static size_t octal_to_bin(const char *str, int size) {
    size_t n = 0;
    for (int i = 0; i < size; i++) {
        if (str[i] < '0' || str[i] > '7') break;
        n = (n << 3) + (str[i] - '0');
    }
    return n;
}

static int tar_read(vfs_node_t *node, uint64_t offset, size_t size, uint8_t *buffer) {
    uint8_t *data_ptr = (uint8_t *)node->device_ptr;
    if (offset >= node->size) return 0;

    size_t to_read = size;
    if (offset + to_read > node->size) {
        to_read = node->size - offset;
    }

    for (size_t i = 0; i < to_read; i++) {
        buffer[i] = data_ptr[offset + i];
    }

    return (int)to_read;
}

static vfs_node_t *tar_finddir(vfs_node_t *node, const char *name) {
    (void)node;
    uint64_t ptr = archive_start;

    while (ptr < (archive_start + archive_len)) {
        struct tar_header *hdr = (struct tar_header *)ptr;
        if (hdr->name[0] == '\0') break;

        size_t file_size = octal_to_bin(hdr->size, 11);

        /* Vérifier correspondance avec le nom */
        const char *t_name = hdr->name;
        int match = 1;
        int i = 0;
        for (; t_name[i] && name[i]; i++) {
            if (t_name[i] != name[i]) { match = 0; break; }
        }
        if (match && (t_name[i] == '\0' || t_name[i] == '/') && name[i] == '\0') {
            vfs_node_t *found = (vfs_node_t *)kmalloc(sizeof(vfs_node_t));
            for (int k = 0; k < 127 && name[k]; k++) found->name[k] = name[k];
            found->name[127] = '\0';
            found->size = (uint32_t)file_size;
            found->flags = (hdr->typeflag == '5') ? VFS_DIRECTORY : VFS_FILE;
            found->device_ptr = (void *)(ptr + 512);
            found->ops = &tar_ops;
            return found;
        }

        /* Sauter l'en-tête + blocs de données arrondis à 512 octets */
        ptr += 512 + ((file_size + 511) & ~511ULL);
    }

    return NULL;
}

void tar_init(uint64_t ramdisk_addr, size_t ramdisk_size) {
    serial_print("[*] Etape 57 : Montage de l Initrd / Ramdisk (format TAR)...\n");
    archive_start = ramdisk_addr;
    archive_len = ramdisk_size;

    serial_print("  -> Dump memoire archive_start: ");
    uint8_t *dbg = (uint8_t *)archive_start;
    for (int i = 0; i < 16; i++) {
        serial_print_hex(dbg[i]);
        serial_print(" ");
    }
    serial_print("\n");

    tar_ops.read = tar_read;
    tar_ops.write = NULL;
    tar_ops.finddir = tar_finddir;

    tar_root.name[0] = 'r';
    tar_root.name[1] = 'd';
    tar_root.name[2] = '\0';
    tar_root.flags = VFS_DIRECTORY;
    tar_root.size = 0;
    tar_root.device_ptr = NULL;
    tar_root.ops = &tar_ops;

    /* Énumération des fichiers dans l'archive */
    uint64_t ptr = archive_start;
    int count = 0;
    while (ptr < (archive_start + archive_len)) {
        struct tar_header *hdr = (struct tar_header *)ptr;
        if (hdr->name[0] == '\0') break;

        size_t sz = octal_to_bin(hdr->size, 11);
        serial_print("  -> Ramdisk Fichier: ");
        serial_print(hdr->name);
        serial_print(" | Taille: ");
        serial_print_dec(sz);
        serial_print(" octets\n");

        count++;
        ptr += 512 + ((sz + 511) & ~511ULL);
    }

    serial_print("[+] Initrd TAR monte avec succes (");
    serial_print_dec(count);
    serial_print(" elements trouves)!\n");
}

vfs_node_t *tar_get_root_node(void) {
    return &tar_root;
}
