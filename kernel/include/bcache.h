#ifndef BCACHE_H
#define BCACHE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define BCACHE_BLOCK_SIZE 512
#define BCACHE_CAPACITY   64 /* 64 blocs = 32 KB de cache en RAM */

typedef struct bcache_entry {
    uint32_t lba;
    bool     valid;
    bool     dirty;
    uint8_t  data[BCACHE_BLOCK_SIZE] __attribute__((aligned(16)));
    struct bcache_entry *prev;
    struct bcache_entry *next;
} bcache_entry_t;

void bcache_init(void);
bool bcache_read(uint32_t lba, uint8_t *buffer);
bool bcache_write(uint32_t lba, const uint8_t *buffer);
void bcache_flush(void);

#endif
