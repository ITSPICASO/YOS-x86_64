#ifndef NET_H
#define NET_H

#include <stdint.h>
#include <stdbool.h>

#define ETH_TYPE_IPV4 0x0800
#define ETH_TYPE_ARP  0x0806

#define ARP_OP_REQUEST 1
#define ARP_OP_REPLY   2

/* Structure Ethernet Frame (14 octets) */
typedef struct __attribute__((packed)) {
    uint8_t  dest_mac[6];
    uint8_t  src_mac[6];
    uint16_t ethertype;
} eth_header_t;

/* Structure ARP Packet (28 octets) */
typedef struct __attribute__((packed)) {
    uint16_t hw_type;       /* 1 = Ethernet */
    uint16_t proto_type;    /* 0x0800 = IPv4 */
    uint8_t  hw_len;        /* 6 */
    uint8_t  proto_len;     /* 4 */
    uint16_t opcode;        /* 1 = Request, 2 = Reply */
    uint8_t  src_mac[6];
    uint32_t src_ip;
    uint8_t  dest_mac[6];
    uint32_t dest_ip;
} arp_packet_t;


#define IP_PROTO_ICMP 1
#define IP_PROTO_TCP  6
#define IP_PROTO_UDP  17

#define ICMP_TYPE_ECHO_REPLY   0
#define ICMP_TYPE_ECHO_REQUEST 8

/* En-tete IPv4 (20 octets min) */
typedef struct __attribute__((packed)) {
    uint8_t  ihl_version;   /* Version (4 bits) + IHL (4 bits) */
    uint8_t  tos;
    uint16_t total_len;
    uint16_t id;
    uint16_t flags_fragment;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t checksum;
    uint32_t src_ip;
    uint32_t dest_ip;
} ipv4_header_t;

/* En-tete ICMP (8 octets min) */
typedef struct __attribute__((packed)) {
    uint8_t  type;
    uint8_t  code;
    uint16_t checksum;
    uint16_t id;
    uint16_t sequence;
} icmp_header_t;


/* En-tete UDP (8 octets) */
typedef struct __attribute__((packed)) {
    uint16_t src_port;
    uint16_t dest_port;
    uint16_t length;
    uint16_t checksum;
} udp_header_t;


#define DHCP_SERVER_PORT 67
#define DHCP_CLIENT_PORT 68

#define DHCP_DISCOVER 1
#define DHCP_OFFER    2
#define DHCP_REQUEST  3
#define DHCP_ACK      5

typedef struct __attribute__((packed)) {
    uint8_t  op;           /* 1 = BOOTREQUEST, 2 = BOOTREPLY */
    uint8_t  htype;        /* 1 = Ethernet */
    uint8_t  hlen;         /* 6 */
    uint8_t  hops;
    uint32_t xid;          /* Transaction ID */
    uint16_t secs;
    uint16_t flags;
    uint32_t ciaddr;       /* Client IP */
    uint32_t yiaddr;       /* Your IP (proposee par le serveur) */
    uint32_t siaddr;       /* Server IP */
    uint32_t giaddr;       /* Relay IP */
    uint8_t  chaddr[16];   /* Client Hardware MAC */
    uint8_t  sname[64];
    uint8_t  file[128];
    uint32_t magic_cookie; /* 0x63825363 */
    uint8_t  options[64];
} dhcp_packet_t;


#define DNS_PORT 53

typedef struct __attribute__((packed)) {
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
} dns_header_t;

int net_dns_resolve(const char *hostname, uint32_t *out_ip);
void dns_handle_packet(const uint8_t *payload, uint16_t len);

void dhcp_discover(void);
void dhcp_handle_packet(const dhcp_packet_t *dhcp, uint16_t len);

int udp_send_packet(uint32_t dest_ip, uint16_t src_port, uint16_t dest_port, const void *data, uint16_t len);
void udp_handle_packet(const eth_header_t *eth, const ipv4_header_t *ip, const uint8_t *payload, uint16_t len);

uint16_t net_checksum(const void *addr, int count);
void ipv4_handle_packet(const eth_header_t *eth, const uint8_t *pkt, uint16_t len);

void net_init(void);
int net_ping(uint32_t dest_ip);
void net_handle_packet(const uint8_t *pkt, uint16_t len);
void arp_handle_packet(const eth_header_t *eth, const arp_packet_t *arp);
void arp_send_reply(const arp_packet_t *req);

/* Conversion Big-Endian <-> Little-Endian */
static inline uint16_t htons(uint16_t v) {
    return (v << 8) | (v >> 8);
}
static inline uint16_t ntohs(uint16_t v) {
    return htons(v);
}
static inline uint32_t htonl(uint32_t v) {
    return ((v >> 24) & 0xFF) | ((v >> 8) & 0xFF00) | ((v << 8) & 0xFF0000) | ((v << 24) & 0xFF000000);
}
static inline uint32_t ntohl(uint32_t v) {
    return htonl(v);
}

#endif
