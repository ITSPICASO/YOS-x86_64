#include "socket.h"
#include "net.h"
#include "serial.h"

static socket_t sockets[MAX_SOCKETS];
static uint16_t dynamic_port = 50000;

void socket_init(void) {
    for (int i = 0; i < MAX_SOCKETS; i++) {
        sockets[i].used = false;
        sockets[i].port = 0;
        sockets[i].has_data = false;
        sockets[i].rx_len = 0;
    }
}

int sys_socket(int domain, int type, int protocol) {
    (void)protocol;
    if (domain != AF_INET || type != SOCK_DGRAM) return -1;

    for (int i = 0; i < MAX_SOCKETS; i++) {
        if (!sockets[i].used) {
            sockets[i].used = true;
            sockets[i].port = dynamic_port++;
            sockets[i].has_data = false;
            sockets[i].rx_len = 0;
            return i;
        }
    }
    return -1;
}

long sys_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr_in *dest_addr) {
    (void)flags;
    if (sockfd < 0 || sockfd >= MAX_SOCKETS || !sockets[sockfd].used || !dest_addr || !buf) return -1;

    uint32_t dest_ip = dest_addr->sin_addr;
    uint16_t dest_port = ntohs(dest_addr->sin_port);
    uint16_t src_port = sockets[sockfd].port;

    int ret = udp_send_packet(dest_ip, src_port, dest_port, buf, (uint16_t)len);
    return (ret == 0) ? (long)len : -1;
}

long sys_recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr_in *src_addr) {
    (void)flags;
    (void)src_addr;
    if (sockfd < 0 || sockfd >= MAX_SOCKETS || !sockets[sockfd].used || !buf) return -1;

    if (!sockets[sockfd].has_data) return 0;

    size_t copy_len = sockets[sockfd].rx_len;
    if (copy_len > len) copy_len = len;

    for (size_t i = 0; i < copy_len; i++) {
        ((uint8_t *)buf)[i] = sockets[sockfd].rx_buffer[i];
    }

    sockets[sockfd].has_data = false;
    sockets[sockfd].rx_len = 0;
    return (long)copy_len;
}

void socket_dispatch_udp(uint16_t dest_port, const uint8_t *data, uint16_t len) {
    for (int i = 0; i < MAX_SOCKETS; i++) {
        if (sockets[i].used && sockets[i].port == dest_port) {
            uint16_t copy_len = len;
            if (copy_len > SOCK_BUF_SIZE) copy_len = SOCK_BUF_SIZE;

            for (uint16_t j = 0; j < copy_len; j++) {
                sockets[i].rx_buffer[j] = data[j];
            }
            sockets[i].rx_len = copy_len;
            sockets[i].has_data = true;
            break;
        }
    }
}
