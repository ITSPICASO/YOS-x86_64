#ifndef _USER_UNISTD_H
#define _USER_UNISTD_H

#include <stddef.h>
#include <stdint.h>

int64_t read(int fd, void *buf, size_t count);
int64_t write(int fd, const void *buf, size_t count);
int     close(int fd);
int     fork(void);
void    yield(void);

#endif
