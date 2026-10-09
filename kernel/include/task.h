#ifndef TASK_H
#define TASK_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define THREAD_READY    0
#define THREAD_RUNNING  1
#define THREAD_SLEEPING 2
#define THREAD_ZOMBIE   3

#define MLFQ_PRIO_HIGH   0
#define MLFQ_PRIO_MEDIUM 1
#define MLFQ_PRIO_LOW    2
#define MLFQ_LEVELS      3

#define STACK_SIZE 4096

#define MAX_PROCESS_FDS 16

#define FD_MODE_READ  0x01
#define FD_MODE_WRITE 0x02

struct vfs_node;

typedef struct file_descriptor {
    struct vfs_node *node;
    uint64_t         offset;
    uint32_t         flags;
    bool             used;
} file_descriptor_t;

struct process;

/* Etape 62 : Structure thread_t */
typedef struct thread {
    uint64_t         rsp;        /* offset 0 */
    uint64_t         user_rsp;   /* offset 8 */
    uint32_t         tid;
    uint32_t         state;
    uint32_t         priority;       /* Etape 65 : MLFQ priority (0..2) */
    uint32_t         time_slice;     /* Ticks restants dans le quantum */
    uint64_t         wake_tick;
    uint8_t         *stack_base;
    struct process  *process;        /* Pointeur vers le processus parent */
    struct thread   *next;
} thread_t;

/* Etape 62 : Structure process_t */
typedef struct process {
    uint32_t           pid;
    int                exit_code;
    struct process    *parent;
    uint64_t           cr3;            /* PML4 virtuel */
    file_descriptor_t  fd_table[MAX_PROCESS_FDS];
    thread_t          *threads;        /* Liste des threads de ce processus */
    struct process    *next;
} process_t;

extern thread_t *current_thread;

void task_init(void);
thread_t *thread_create(void (*entry_point)(void), uint32_t priority);
void task_yield(void);
void task_sleep(uint64_t ms);
void thread_exit(void);
void reaper_thread_func(void);
uint64_t task_schedule_from_timer(uint64_t current_rsp);

extern void switch_context(uint64_t *old_rsp, uint64_t new_rsp);
extern void enter_user_mode(uint64_t entry_point, uint64_t user_rsp);


process_t *process_create(uint64_t cr3);
int64_t task_fork(uint64_t user_rsp, uint64_t user_rip);
int64_t task_waitpid(int32_t pid, int *status);

#endif
