#ifndef GPT_H
#define GPT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    uint8_t bytes[16];
} __attribute__((packed)) gpt_guid_t;

typedef struct {
    char        signature[8];       /* "EFI PART" */
    uint32_t    revision;
    uint32_t    header_size;
    uint32_t    header_crc32;
    uint32_t    reserved;
    uint64_t    my_lba;
    uint64_t    alternate_lba;
    uint64_t    first_usable_lba;
    uint64_t    last_usable_lba;
    gpt_guid_t  disk_guid;
    uint64_t    partition_entry_lba;
    uint32_t    num_partition_entries;
    uint32_t    sizeof_partition_entry;
    uint32_t    partition_entry_array_crc32;
} __attribute__((packed)) gpt_header_t;

typedef struct {
    gpt_guid_t  type_guid;
    gpt_guid_t  unique_partition_guid;
    uint64_t    starting_lba;
    uint64_t    ending_lba;
    uint64_t    attributes;
    uint16_t    partition_name[36]; /* UTF-16LE */
} __attribute__((packed)) gpt_entry_t;

uint32_t crc32(const void *data, size_t n_bytes);
int gpt_parse(const uint8_t *lba1_buffer);

#endif
