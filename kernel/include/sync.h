#ifndef SYNC_H
#define SYNC_H

#include <stdint.h>
#include "spinlock.h"

typedef struct {
    volatile int32_t value;
    spinlock_t lock;
} semaphore_t;

static inline void sem_init(semaphore_t *sem, int32_t initial_value) {
    sem->value = initial_value;
    spinlock_init(&sem->lock);
}

static inline void sem_wait(semaphore_t *sem) {
    for (;;) {
        spinlock_acquire(&sem->lock);
        if (sem->value > 0) {
            sem->value--;
            spinlock_release(&sem->lock);
            break;
        }
        spinlock_release(&sem->lock);
        __asm__ volatile("pause");
    }
}

static inline void sem_post(semaphore_t *sem) {
    spinlock_acquire(&sem->lock);
    sem->value++;
    spinlock_release(&sem->lock);
}

#endif
