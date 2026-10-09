#include "e1000.h"
#include "wm.h"
#include "tss.h"
#include "vmm.h"
#include "task.h"
#include "heap.h"
#include "serial.h"
#include "apic.h"
#include "slab.h"

static process_t kernel_proc;
static thread_t  kernel_main_thread;
thread_t *current_thread = NULL;
static thread_t *thread_queue = NULL;

static uint32_t next_tid = 1;
static uint64_t sched_ticks = 0;

void reaper_thread_func(void) {
    for (;;) {
        /* Parcourir la liste pour nettoyer les zombies */
        thread_t *prev = NULL;
        thread_t *curr = thread_queue;
        while (curr) {
            if (curr->state == THREAD_ZOMBIE && curr != &kernel_main_thread) {
                serial_print("[REAPER] Nettoyage memoire thread TID ");
                serial_print_dec(curr->tid);
                serial_print("\n");

                if (prev) prev->next = curr->next;
                else thread_queue = curr->next;

                if (curr->stack_base) kfree(curr->stack_base);
                thread_t *to_free = curr;
                curr = curr->next;
                kfree(to_free);
                continue;
            }
            prev = curr;
            curr = curr->next;
        }
        task_sleep(500);
    }
}

void task_init(void) {
    serial_print("[*] Etape 61 & 62 : Initialisation du multitache (Process & Threads)...\n");

    kernel_proc.pid = 0;
    kernel_proc.cr3 = 0;
    kernel_proc.threads = &kernel_main_thread;
    kernel_proc.next = NULL;

    kernel_main_thread.tid = 0;
    kernel_main_thread.state = THREAD_RUNNING;
    kernel_main_thread.priority = MLFQ_PRIO_HIGH;
    kernel_main_thread.time_slice = 10;
    kernel_main_thread.rsp = 0;
    kernel_main_thread.wake_tick = 0;
    kernel_main_thread.stack_base = NULL;
    kernel_main_thread.process = &kernel_proc;
    kernel_main_thread.next = NULL;

    current_thread = &kernel_main_thread;
    thread_queue = &kernel_main_thread;

    /* Lancer le thread faucheur (Etape 67) */
    thread_create(reaper_thread_func, MLFQ_PRIO_LOW);

    serial_print("[+] Processus Kernel (PID 0) & Reaper Thread configures.\n");
}

thread_t *thread_create(void (*entry_point)(void), uint32_t priority) {
    thread_t *nt = (thread_t *)kmalloc(sizeof(thread_t));
    nt->stack_base = (uint8_t *)kmalloc(STACK_SIZE);
    nt->tid = next_tid++;
    nt->state = THREAD_READY;
    nt->priority = priority;
    nt->time_slice = (priority == MLFQ_PRIO_HIGH) ? 5 : ((priority == MLFQ_PRIO_MEDIUM) ? 10 : 20);
    nt->wake_tick = 0;
    nt->process = &kernel_proc;

    uint64_t top = ((uint64_t)(nt->stack_base + STACK_SIZE - 16)) & ~0xFULL;
    uint64_t *st = (uint64_t *)top;

    *(--st) = 0x10;
    *(--st) = top;
    *(--st) = 0x202;
    *(--st) = 0x08;
    *(--st) = (uint64_t)entry_point;

    for (int i = 0; i < 15; i++) *(--st) = 0;

    nt->rsp = (uint64_t)st;

    /* Ajouter a la file */
    nt->next = thread_queue;
    thread_queue = nt;

    serial_print("[+] Nouveau Thread cree | TID: ");
    serial_print_dec(nt->tid);
    serial_print(" (Priorite: ");
    serial_print_dec(priority);
    serial_print(")\n");

    return nt;
}

void task_sleep(uint64_t ms) {
    if (!current_thread) return;
    uint64_t ticks = ms / 20;
    if (ticks == 0) ticks = 1;
    current_thread->wake_tick = sched_ticks + ticks;
    current_thread->state = THREAD_SLEEPING;
    task_yield();
}

void thread_exit(void) {
    if (!current_thread) return;
    current_thread->state = THREAD_ZOMBIE;
    task_yield();
    for (;;) __asm__ volatile("hlt");
}

/* Etape 65 : Scheduler MLFQ avec APIC Timer */
uint64_t task_schedule_from_timer(uint64_t current_rsp) {
    lapic_eoi();
    sched_ticks++;
    e1000_poll_rx();

    /* Actualisation de l horloge Taskbar chaque seconde (50 Hz) */
    if ((sched_ticks % 50) == 0) {
        wm_update_clock();
    }

    if (!current_thread) return current_rsp;

    current_thread->rsp = current_rsp;
    if (current_thread->state == THREAD_RUNNING) {
        current_thread->state = THREAD_READY;
    }

    /* Reveil des threads dormeurs */
    thread_t *it = thread_queue;
    while (it) {
        if (it->state == THREAD_SLEEPING && sched_ticks >= it->wake_tick) {
            it->state = THREAD_READY;
        }
        it = it->next;
    }

    /* Recherche du prochain thread pret selon la plus haute priorite MLFQ */
    thread_t *best = NULL;
    for (int prio = MLFQ_PRIO_HIGH; prio <= MLFQ_PRIO_LOW; prio++) {
        it = thread_queue;
        while (it) {
            if (it->state == THREAD_READY && it->priority == (uint32_t)prio && it != current_thread) {
                best = it;
                break;
            }
            it = it->next;
        }
        if (best) break;
    }

    if (!best) {
        if (current_thread->state == THREAD_READY) best = current_thread;
        else best = &kernel_main_thread;
    }

    current_thread = best;
    current_thread->state = THREAD_RUNNING;

    if (current_thread->stack_base) {
        tss_set_rsp0((uint64_t)current_thread->stack_base + STACK_SIZE);
    }

    if (current_thread->process && current_thread->process->cr3 != 0) {
        uint64_t current_cr3;
        __asm__ volatile("mov %%cr3, %0" : "=r"(current_cr3));
        if (current_cr3 != current_thread->process->cr3) {
            __asm__ volatile("mov %0, %%cr3" : : "r"(current_thread->process->cr3) : "memory");
        }
    }

    return current_thread->rsp;
}

void task_yield(void) {
    __asm__ volatile("int $0x20");
}

static uint32_t next_pid = 1;
static process_t *process_list = &kernel_proc;

process_t *process_create(uint64_t cr3) {
    process_t *p = (process_t *)kmalloc(sizeof(process_t));
    if (!p) return NULL;
    p->pid = next_pid++;
    p->cr3 = cr3;
    p->exit_code = 0;
    p->threads = NULL;
    p->parent = (current_thread && current_thread->process) ? current_thread->process : &kernel_proc;
    p->next = process_list;
    process_list = p;

    for (int i = 0; i < MAX_PROCESS_FDS; i++) {
        p->fd_table[i].used = false;
        p->fd_table[i].node = NULL;
        p->fd_table[i].offset = 0;
    }
    return p;
}


int64_t task_fork(uint64_t user_rsp, uint64_t user_rip) {
    if (!current_thread || !current_thread->process) return -1;

    uint64_t child_phys_cr3 = 0;
    page_table_t *child_pml4 = vmm_clone_address_space((page_table_t *)current_thread->process->cr3, &child_phys_cr3);
    if (!child_pml4) {
        serial_print("[-] Fork: Echec clonage espace d'adressage.\n");
        return -1;
    }

    process_t *child_proc = process_create(child_phys_cr3);
    if (!child_proc) return -1;

    /* Duplication des FDs */
    for (int i = 0; i < MAX_PROCESS_FDS; i++) {
        child_proc->fd_table[i] = current_thread->process->fd_table[i];
    }

    /* Creation du thread enfant */
    thread_t *child_thread = (thread_t *)kmalloc(sizeof(thread_t));
    child_thread->stack_base = (uint8_t *)kmalloc(STACK_SIZE);
    child_thread->tid = next_tid++;
    child_thread->state = THREAD_READY;
    child_thread->priority = current_thread->priority;
    child_thread->time_slice = 10;
    child_thread->wake_tick = 0;
    child_thread->process = child_proc;

    /* Stack frame compatible avec switch_context */
    uint64_t top = ((uint64_t)(child_thread->stack_base + STACK_SIZE - 16)) & ~0xFULL;
    uint64_t *st = (uint64_t *)top;

    /* Frame matérielle dépilée par iretq dans apic_timer_stub */
    *(--st) = 0x1B;                     /* SS : User Data (0x18 | 3) */
    *(--st) = user_rsp;                 /* RSP réel au moment du fork */
    *(--st) = 0x202;                    /* RFLAGS : IF=1 */
    *(--st) = 0x23;                     /* CS : User Code (0x20 | 3) */
    *(--st) = user_rip;                 /* RIP réel de l'instruction suivant le syscall */

    /* 15 registres généraux dépilés par apic_timer_stub (pop r15 .. pop rax) */
    *(--st) = 0; /* R15 */
    *(--st) = 0; /* R14 */
    *(--st) = 0; /* R13 */
    *(--st) = 0; /* R12 */
    *(--st) = 0; /* R11 */
    *(--st) = 0; /* R10 */
    *(--st) = 0; /* R9  */
    *(--st) = 0; /* R8  */
    *(--st) = 0; /* RBP */
    *(--st) = 0; /* RDI */
    *(--st) = 0; /* RSI */
    *(--st) = 0; /* RDX */
    *(--st) = 0; /* RCX */
    *(--st) = 0; /* RBX */
    *(--st) = 0; /* RAX = 0 (valeur de retour pour le child) */

    child_thread->rsp = (uint64_t)st;
    child_thread->next = thread_queue;
    thread_queue = child_thread;

    serial_print("[+] fork(): Nouveau processus cree PID: ");
    serial_print_dec(child_proc->pid);
    serial_print(" | TID: ");
    serial_print_dec(child_thread->tid);
    serial_print("\n");

    return (int64_t)child_proc->pid;
}

int64_t task_waitpid(int32_t pid, int *status) {
    (void)status;
    for (;;) {
        bool running = false;
        thread_t *t = thread_queue;
        while (t) {
            if (t->process && (int32_t)t->process->pid == pid) {
                if (t->state != THREAD_ZOMBIE) {
                    running = true;
                    break;
                }
            }
            t = t->next;
        }
        if (!running) break;

        /* Activer les interruptions temporairement pour laisser l'APIC timer scheduler tourner */
        __asm__ volatile("sti");
        for (volatile int i = 0; i < 100000; i++) {
            __asm__ volatile("pause");
        }
        __asm__ volatile("cli");
    }
    return pid;
}
