#include "heap.h"
#include "serial.h"

static heap_block_t *head_block = 0;

void heap_init(void) {
    head_block = (heap_block_t *)KERNEL_HEAP_START;
    head_block->size = KERNEL_HEAP_INITIAL_SIZE - sizeof(heap_block_t);
    head_block->is_free = 1;
    head_block->next = 0;
    head_block->prev = 0;
}

void *kmalloc(size_t size) {
    if (size == 0) return 0;

    /* Alignement a 16 octets */
    size = (size + 15) & ~15;

    heap_block_t *curr = head_block;
    while (curr) {
        if (curr->is_free && curr->size >= size) {
            if (curr->size >= size + sizeof(heap_block_t) + 16) {
                heap_block_t *next_block = (heap_block_t *)((uint8_t *)curr + sizeof(heap_block_t) + size);
                next_block->size = curr->size - size - sizeof(heap_block_t);
                next_block->is_free = 1;
                next_block->next = curr->next;
                next_block->prev = curr;

                if (curr->next) curr->next->prev = next_block;
                curr->next = next_block;
                curr->size = size;
            }
            curr->is_free = 0;
            return (void *)((uint8_t *)curr + sizeof(heap_block_t));
        }
        curr = curr->next;
    }
    return 0;
}

void kfree(void *ptr) {
    if (!ptr) return;

    heap_block_t *block = (heap_block_t *)((uint8_t *)ptr - sizeof(heap_block_t));
    block->is_free = 1;

    if (block->next && block->next->is_free) {
        block->size += sizeof(heap_block_t) + block->next->size;
        block->next = block->next->next;
        if (block->next) block->next->prev = block;
    }

    if (block->prev && block->prev->is_free) {
        block->prev->size += sizeof(heap_block_t) + block->size;
        block->prev->next = block->next;
        if (block->next) block->next->prev = block->prev;
    }
}
