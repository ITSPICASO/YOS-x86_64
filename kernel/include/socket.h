#ifndef SOCKET_H
#define SOCKET_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define AF_INET     2
#define SOCK_DGRAM  2
#define IPPROTO_UDP 17

struct sockaddr_in {
    uint16_t sin_family;
    uint16_t sin_port;
    uint32_t sin_addr;
    char     sin_zero[8];
};

#define MAX_SOCKETS 16
#define SOCK_BUF_SIZE 2048

typedef struct {
    bool     used;
    uint16_t port;
    uint8_t  rx_buffer[SOCK_BUF_SIZE];
    uint16_t rx_len;
    bool     has_data;
} socket_t;

void socket_init(void);
int  sys_socket(int domain, int type, int protocol);
long sys_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr_in *dest_addr);
long sys_recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr_in *src_addr);
void socket_dispatch_udp(uint16_t dest_port, const uint8_t *data, uint16_t len);

#endif
