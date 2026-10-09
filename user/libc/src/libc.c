#include "syscalls.h"
#include "string.h"
#include "stdio.h"
#include "stdlib.h"
#include "unistd.h"
#include <stdarg.h>

/* String operations */
size_t strlen(const char *s) {
    size_t len = 0;
    while (s && s[len]) len++;
    return len;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++; s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) {
        s1++; s2++; n--;
    }
    if (n == 0) return 0;
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

void *memset(void *s, int c, size_t n) {
    uint8_t *p = (uint8_t *)s;
    while (n--) *p++ = (uint8_t)c;
    return s;
}

void *memcpy(void *dest, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    while (n--) *d++ = *s++;
    return dest;
}

/* System calls wrappers */
int64_t write(int fd, const void *buf, size_t count) {
    return syscall3(SYS_WRITE, fd, (int64_t)buf, count);
}

int64_t read(int fd, void *buf, size_t count) {
    return syscall3(SYS_READ, fd, (int64_t)buf, count);
}

int close(int fd) {
    return (int)syscall1(SYS_CLOSE, fd);
}

int fork(void) {
    return (int)syscall0(SYS_FORK);
}

void yield(void) {
    syscall0(SYS_YIELD);
}

void exit(int status) {
    syscall1(SYS_EXIT, status);
    while (1);
}

/* I/O formatted */
void putchar(char c) {
    write(1, &c, 1);
}

void puts(const char *s) {
    write(1, s, strlen(s));
    putchar('\n');
}

static void print_dec(int64_t val) {
    if (val < 0) {
        putchar('-');
        val = -val;
    }
    char buf[24];
    int i = 0;
    if (val == 0) {
        putchar('0');
        return;
    }
    while (val > 0) {
        buf[i++] = (val % 10) + '0';
        val /= 10;
    }
    while (i > 0) putchar(buf[--i]);
}

static void print_hex(uint64_t val) {
    const char hex[] = "0123456789ABCDEF";
    char buf[16];
    int i = 0;
    if (val == 0) {
        putchar('0');
        return;
    }
    while (val > 0) {
        buf[i++] = hex[val & 0xF];
        val >>= 4;
    }
    while (i > 0) putchar(buf[--i]);
}

void printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    for (size_t i = 0; fmt[i]; i++) {
        if (fmt[i] == '%' && fmt[i+1]) {
            i++;
            if (fmt[i] == 's') {
                const char *s = va_arg(args, const char *);
                if (!s) s = "(null)";
                write(1, s, strlen(s));
            } else if (fmt[i] == 'd') {
                print_dec(va_arg(args, int));
            } else if (fmt[i] == 'x') {
                print_hex(va_arg(args, uint64_t));
            } else if (fmt[i] == 'c') {
                putchar((char)va_arg(args, int));
            } else if (fmt[i] == '%') {
                putchar('%');
            }
        } else {
            putchar(fmt[i]);
        }
    }
    va_end(args);
}

int atoi(const char *str) {
    int res = 0, sign = 1;
    while (*str == ' ') str++;
    if (*str == '-') { sign = -1; str++; }
    else if (*str == '+') str++;
    while (*str >= '0' && *str <= '9') {
        res = res * 10 + (*str - '0');
        str++;
    }
    return res * sign;
}
