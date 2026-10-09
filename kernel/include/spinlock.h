#ifndef SPINLOCK_H
#define SPINLOCK_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    volatile uint32_t lock;
} spinlock_t;

#define SPINLOCK_INIT { .lock = 0 }

static inline void spinlock_init(spinlock_t *lk) {
    lk->lock = 0;
}

static inline void spinlock_acquire(spinlock_t *lk) {
    while (__atomic_test_and_set(&(lk->lock), __ATOMIC_ACQUIRE)) {
        __asm__ volatile("pause");
    }
}

static inline void spinlock_release(spinlock_t *lk) {
    __atomic_clear(&(lk->lock), __ATOMIC_RELEASE);
}

#endif
