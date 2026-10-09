#ifndef HEAP_H
#define HEAP_H

#include <stdint.h>
#include <stddef.h>

#define KERNEL_HEAP_START 0xFFFFFFFF81000000ULL
#define KERNEL_HEAP_INITIAL_SIZE (32 * 1024 * 1024ULL) /* 2 Mo */

typedef struct heap_block {
    uint64_t size;
    uint8_t is_free;
    struct heap_block *next;
    struct heap_block *prev;
} heap_block_t;

void heap_init(void);
void *kmalloc(size_t size);
void kfree(void *ptr);

#endif
