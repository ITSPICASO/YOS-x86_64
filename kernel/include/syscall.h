#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define IA32_EFER    0xC0000080
#define IA32_STAR    0xC0000081
#define IA32_LSTAR   0xC0000082
#define IA32_SFMASK  0xC0000084

#define EFER_SCE     (1ULL << 0)

/* Syscall numbers (Etape 70, 71, 94) */
#define SYS_EXIT     0
#define SYS_FORK     1
#define SYS_READ     2
#define SYS_WRITE    3
#define SYS_OPEN     4
#define SYS_CLOSE    5
#define SYS_WAITPID  6
#define SYS_EXECVE   7
#define SYS_SEEK     8
#define SYS_STAT     9
#define SYS_YIELD    10

/* BSD Sockets API */
#define SYS_SOCKET   20
#define SYS_SENDTO   21
#define SYS_RECVFROM 22

#define MAX_SYSCALLS 32

void syscall_init(void);
extern void syscall_entry(void);

/* Etape 69: Pointer validation & memory copying */
bool copy_from_user(void *dst, const void *src, size_t n);
bool copy_to_user(void *dst, const void *src, size_t n);

#endif
