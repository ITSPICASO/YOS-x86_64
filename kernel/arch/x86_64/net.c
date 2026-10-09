#include "socket.h"
#include "net.h"
#include "e1000.h"
#include "serial.h"

/* IP statique / actuelle pour QEMU SLIRP : 10.0.2.15 */
static uint32_t guest_ip = (10) | (0 << 8) | (2 << 16) | (15 << 24);

uint16_t net_checksum(const void *addr, int count) {
    uint32_t sum = 0;
    const uint16_t *ptr = (const uint16_t *)addr;

    while (count > 1) {
        sum += *ptr++;
        count -= 2;
    }
    if (count > 0) {
        sum += *(const uint8_t *)ptr;
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return (uint16_t)(~sum);
}

void net_init(void) {
    serial_print("[+] Pile Reseau (Bloc 14) : Initialisee avec IP 10.0.2.15\n");
}

/* ========================================================================= */
/*                              ARP ENGINE                                   */
/* ========================================================================= */

void arp_send_reply(const arp_packet_t *req) {
    e1000_device_t *nic = e1000_get_device();
    if (!nic) return;

    struct __attribute__((packed)) {
        eth_header_t eth;
        arp_packet_t arp;
    } reply;

    for (int i = 0; i < 6; i++) {
        reply.eth.dest_mac[i] = req->src_mac[i];
        reply.eth.src_mac[i]  = nic->mac[i];
    }
    reply.eth.ethertype = htons(ETH_TYPE_ARP);

    reply.arp.hw_type    = htons(1);
    reply.arp.proto_type = htons(ETH_TYPE_IPV4);
    reply.arp.hw_len     = 6;
    reply.arp.proto_len  = 4;
    reply.arp.opcode     = htons(ARP_OP_REPLY);

    for (int i = 0; i < 6; i++) {
        reply.arp.src_mac[i]  = nic->mac[i];
        reply.arp.dest_mac[i] = req->src_mac[i];
    }
    reply.arp.src_ip  = guest_ip;
    reply.arp.dest_ip = req->src_ip;

    e1000_send_packet(&reply, sizeof(reply));
    serial_print("[+] ARP: Reponse (ARP Reply) envoyee avec succes!\n");

    /* Relancer le Ping maintenant que la passerelle nous connait */
    net_ping((10) | (0 << 8) | (2 << 16) | (2 << 24));
}

void arp_handle_packet(const eth_header_t *eth, const arp_packet_t *arp) {
    (void)eth;
    uint16_t op = ntohs(arp->opcode);

    if (op == ARP_OP_REQUEST) {
        if (arp->dest_ip == guest_ip) {
            serial_print("[*] ARP: Requete recue pour notre IP (10.0.2.15). Envoi du Reply...\n");
            arp_send_reply(arp);
        }
    } else if (op == ARP_OP_REPLY) {
        serial_print("[+] ARP: Reponse recue.\n");
    }
}

/* ========================================================================= */
/*                              ICMP & PING                                  */
/* ========================================================================= */

int net_ping(uint32_t dest_ip) {
    e1000_device_t *nic = e1000_get_device();
    if (!nic) return -1;

    static uint8_t ping_buf[64];
    eth_header_t *eth = (eth_header_t *)ping_buf;
    ipv4_header_t *ip = (ipv4_header_t *)(ping_buf + sizeof(eth_header_t));
    icmp_header_t *icmp = (icmp_header_t *)(ping_buf + sizeof(eth_header_t) + sizeof(ipv4_header_t));

    eth->dest_mac[0] = 0x52;
    eth->dest_mac[1] = 0x55;
    eth->dest_mac[2] = 0x0A;
    eth->dest_mac[3] = 0x00;
    eth->dest_mac[4] = 0x02;
    eth->dest_mac[5] = 0x02;
    for (int i = 0; i < 6; i++) eth->src_mac[i] = nic->mac[i];
    eth->ethertype = htons(ETH_TYPE_IPV4);

    ip->ihl_version    = (4 << 4) | (sizeof(ipv4_header_t) / 4);
    ip->tos            = 0;
    ip->total_len      = htons(sizeof(ipv4_header_t) + sizeof(icmp_header_t));
    ip->id             = htons(0xCAFE);
    ip->flags_fragment = 0;
    ip->ttl            = 64;
    ip->protocol       = IP_PROTO_ICMP;
    ip->checksum       = 0;
    ip->src_ip         = guest_ip;
    ip->dest_ip        = dest_ip;
    ip->checksum       = net_checksum(ip, sizeof(ipv4_header_t));

    icmp->type     = ICMP_TYPE_ECHO_REQUEST;
    icmp->code     = 0;
    icmp->checksum = 0;
    icmp->id       = htons(0x4242);
    icmp->sequence = htons(1);
    icmp->checksum = net_checksum(icmp, sizeof(icmp_header_t));

    serial_print("[*] NET: Envoi Ping ICMP vers 10.0.2.2...\n");
    return e1000_send_packet(ping_buf, 60);
}

void icmp_handle_packet(const eth_header_t *eth, const ipv4_header_t *ip, const uint8_t *payload, uint16_t len) {
    if (len < sizeof(icmp_header_t)) return;

    const icmp_header_t *icmp = (const icmp_header_t *)payload;

    if (icmp->type == ICMP_TYPE_ECHO_REPLY) {
        serial_print("[+] ICMP: Echo Reply (Pong) recu de 10.0.2.2! Connexion reseau confirmee!\n");
        return;
    }

    if (icmp->type == ICMP_TYPE_ECHO_REQUEST) {
        serial_print("[+] ICMP: Requete Echo (Ping) recue! Envoi du Reply...\n");

        e1000_device_t *nic = e1000_get_device();
        if (!nic) return;

        static uint8_t reply_buf[1514];
        eth_header_t *rep_eth = (eth_header_t *)reply_buf;
        ipv4_header_t *rep_ip = (ipv4_header_t *)(reply_buf + sizeof(eth_header_t));
        icmp_header_t *rep_icmp = (icmp_header_t *)(reply_buf + sizeof(eth_header_t) + sizeof(ipv4_header_t));

        for (int i = 0; i < 6; i++) {
            rep_eth->dest_mac[i] = eth->src_mac[i];
            rep_eth->src_mac[i]  = nic->mac[i];
        }
        rep_eth->ethertype = htons(ETH_TYPE_IPV4);

        rep_ip->ihl_version    = (4 << 4) | (sizeof(ipv4_header_t) / 4);
        rep_ip->tos            = 0;
        rep_ip->total_len      = htons(sizeof(ipv4_header_t) + len);
        rep_ip->id             = htons(0x1337);
        rep_ip->flags_fragment = 0;
        rep_ip->ttl            = 64;
        rep_ip->protocol       = IP_PROTO_ICMP;
        rep_ip->checksum       = 0;
        rep_ip->src_ip         = guest_ip;
        rep_ip->dest_ip        = ip->src_ip;
        rep_ip->checksum       = net_checksum(rep_ip, sizeof(ipv4_header_t));

        for (uint16_t i = 0; i < len; i++) {
            ((uint8_t *)rep_icmp)[i] = payload[i];
        }
        rep_icmp->type     = ICMP_TYPE_ECHO_REPLY;
        rep_icmp->checksum = 0;
        rep_icmp->checksum = net_checksum(rep_icmp, len);

        uint16_t total_frame_len = sizeof(eth_header_t) + sizeof(ipv4_header_t) + len;
        if (total_frame_len < 60) total_frame_len = 60;

        e1000_send_packet(reply_buf, total_frame_len);
        serial_print("[+] ICMP: Echo Reply envoye avec succes!\n");
    }
}

/* ========================================================================= */
/*                              UDP & DHCP                                   */
/* ========================================================================= */

int udp_send_packet(uint32_t dest_ip, uint16_t src_port, uint16_t dest_port, const void *data, uint16_t len) {
    e1000_device_t *nic = e1000_get_device();
    if (!nic) return -1;

    static uint8_t pkt_buf[1514];
    eth_header_t *eth = (eth_header_t *)pkt_buf;
    ipv4_header_t *ip = (ipv4_header_t *)(pkt_buf + sizeof(eth_header_t));
    udp_header_t *udp = (udp_header_t *)(pkt_buf + sizeof(eth_header_t) + sizeof(ipv4_header_t));
    uint8_t *udp_data = pkt_buf + sizeof(eth_header_t) + sizeof(ipv4_header_t) + sizeof(udp_header_t);

    if (dest_ip == 0xFFFFFFFF) {
        for (int i = 0; i < 6; i++) eth->dest_mac[i] = 0xFF;
    } else {
        eth->dest_mac[0] = 0x52; eth->dest_mac[1] = 0x55; eth->dest_mac[2] = 0x0A;
        eth->dest_mac[3] = 0x00; eth->dest_mac[4] = 0x02; eth->dest_mac[5] = 0x02;
    }
    for (int i = 0; i < 6; i++) eth->src_mac[i] = nic->mac[i];
    eth->ethertype = htons(ETH_TYPE_IPV4);

    uint16_t total_ip_len = sizeof(ipv4_header_t) + sizeof(udp_header_t) + len;
    ip->ihl_version = (4 << 4) | (sizeof(ipv4_header_t) / 4);
    ip->tos = 0;
    ip->total_len = htons(total_ip_len);
    ip->id = htons(0x5678);
    ip->flags_fragment = 0;
    ip->ttl = 64;
    ip->protocol = IP_PROTO_UDP;
    ip->checksum = 0;
    ip->src_ip = guest_ip;
    ip->dest_ip = dest_ip;
    ip->checksum = net_checksum(ip, sizeof(ipv4_header_t));

    udp->src_port = htons(src_port);
    udp->dest_port = htons(dest_port);
    udp->length = htons(sizeof(udp_header_t) + len);
    udp->checksum = 0;

    for (uint16_t i = 0; i < len; i++) {
        udp_data[i] = ((const uint8_t *)data)[i];
    }

    uint16_t total_len = sizeof(eth_header_t) + total_ip_len;
    if (total_len < 60) total_len = 60;

    return e1000_send_packet(pkt_buf, total_len);
}

void dhcp_discover(void) {
    e1000_device_t *nic = e1000_get_device();
    if (!nic) return;

    dhcp_packet_t pkt;
    for (uint16_t i = 0; i < sizeof(dhcp_packet_t); i++) {
        ((uint8_t *)&pkt)[i] = 0;
    }

    pkt.op = 1;      /* BOOTREQUEST */
    pkt.htype = 1;   /* Ethernet */
    pkt.hlen = 6;
    pkt.xid = htonl(0x1337BEEF);
    pkt.flags = htons(0x8000); /* Broadcast flag */

    for (int i = 0; i < 6; i++) {
        pkt.chaddr[i] = nic->mac[i];
    }

    pkt.magic_cookie = htonl(0x63825363);

    /* Options */
    pkt.options[0] = 53;
    pkt.options[1] = 1;
    pkt.options[2] = DHCP_DISCOVER;
    pkt.options[3] = 55;
    pkt.options[4] = 3;
    pkt.options[5] = 1;  /* Subnet Mask */
    pkt.options[6] = 3;  /* Router */
    pkt.options[7] = 6;  /* DNS */
    pkt.options[8] = 255; /* End */

    serial_print("[*] DHCP: Diffusion d une requete DHCP Discover...\n");
    udp_send_packet(0xFFFFFFFF, DHCP_CLIENT_PORT, DHCP_SERVER_PORT, &pkt, sizeof(dhcp_packet_t));
}

void dhcp_handle_packet(const dhcp_packet_t *dhcp, uint16_t len) {
    if (len < sizeof(dhcp_packet_t)) return;
    if (ntohl(dhcp->magic_cookie) != 0x63825363) return;

    if (dhcp->op == 2) { /* BOOTREPLY */
        uint32_t offered_ip = dhcp->yiaddr;
        serial_print("[+] DHCP: Offre recue (DHCP Offer / ACK)!\n");
        serial_print("    -> IP Proposee : ");
        serial_print_dec(offered_ip & 0xFF); serial_print(".");
        serial_print_dec((offered_ip >> 8) & 0xFF); serial_print(".");
        serial_print_dec((offered_ip >> 16) & 0xFF); serial_print(".");
        serial_print_dec((offered_ip >> 24) & 0xFF); serial_print("\n");
        guest_ip = offered_ip;
    }
}


/* DNS Resolver */
static uint32_t resolved_dns_ip = 0;
static bool dns_done = false;

static void encode_dns_name(uint8_t *dest, const char *src) {
    int len_idx = 0;
    int count = 0;
    int i = 0;

    dest[0] = 0;
    int d = 1;

    while (src[i]) {
        if (src[i] == '.') {
            dest[len_idx] = count;
            len_idx = d++;
            count = 0;
        } else {
            dest[d++] = src[i];
            count++;
        }
        i++;
    }
    dest[len_idx] = count;
    dest[d] = 0;
}

int net_dns_resolve(const char *hostname, uint32_t *out_ip) {
    if (!hostname || !out_ip) return -1;

    dns_done = false;
    resolved_dns_ip = 0;

    static uint8_t query_buf[512];
    dns_header_t *dns = (dns_header_t *)query_buf;

    dns->id = htons(0x1337);
    dns->flags = htons(0x0100); /* Standard recursive query */
    dns->qdcount = htons(1);
    dns->ancount = 0;
    dns->nscount = 0;
    dns->arcount = 0;

    uint8_t *qname = query_buf + sizeof(dns_header_t);
    encode_dns_name(qname, hostname);

    int qname_len = 0;
    while (qname[qname_len]) qname_len++;
    qname_len++; /* Null terminator */

    uint16_t *qtype = (uint16_t *)(qname + qname_len);
    qtype[0] = htons(1); /* Type A */
    qtype[1] = htons(1); /* Class IN */

    uint16_t total_dns_len = sizeof(dns_header_t) + qname_len + 4;

    serial_print("[*] DNS: Resolution du domaine '");
    serial_print(hostname);
    serial_print("' vers DNS 10.0.2.3...\n");

    /* DNS Server QEMU SLIRP = 10.0.2.3 */
    uint32_t dns_server_ip = (10) | (0 << 8) | (2 << 16) | (3 << 24);
    udp_send_packet(dns_server_ip, 45678, DNS_PORT, query_buf, total_dns_len);

    return 0;
}

void dns_handle_packet(const uint8_t *payload, uint16_t len) {
    if (len < sizeof(dns_header_t)) return;

    const dns_header_t *dns = (const dns_header_t *)payload;
    if (ntohs(dns->ancount) == 0) return;

    /* Parser la reponse basique : sauter QNAME et lire Answer RDATA */
    const uint8_t *ptr = payload + sizeof(dns_header_t);

    /* Skip Question */
    while (*ptr != 0) ptr++;
    ptr += 5; /* Null + QTYPE(2) + QCLASS(2) */

    /* Skip Answer Name (généralement pointeur 2 octets 0xC0xx) */
    if ((*ptr & 0xC0) == 0xC0) {
        ptr += 2;
    } else {
        while (*ptr != 0) ptr++;
        ptr++;
    }

    uint16_t type = ntohs(*(const uint16_t *)ptr); ptr += 2;
    ptr += 2; /* Class */
    ptr += 4; /* TTL */
    uint16_t data_len = ntohs(*(const uint16_t *)ptr); ptr += 2;

    if (type == 1 && data_len == 4) { /* Type A (IPv4) */
        resolved_dns_ip = *(const uint32_t *)ptr;
        dns_done = true;

        serial_print("[+] DNS: Domaine resolu avec succes! IP: ");
        serial_print_dec(resolved_dns_ip & 0xFF); serial_print(".");
        serial_print_dec((resolved_dns_ip >> 8) & 0xFF); serial_print(".");
        serial_print_dec((resolved_dns_ip >> 16) & 0xFF); serial_print(".");
        serial_print_dec((resolved_dns_ip >> 24) & 0xFF); serial_print("\n");
    }
}

void udp_handle_packet(const eth_header_t *eth, const ipv4_header_t *ip, const uint8_t *payload, uint16_t len) {
    (void)eth;
    (void)ip;
    if (len < sizeof(udp_header_t)) return;

    const udp_header_t *udp = (const udp_header_t *)payload;
    uint16_t src_port = ntohs(udp->src_port);
    uint16_t dst_port = ntohs(udp->dest_port);
    uint16_t ulen = ntohs(udp->length);

    serial_print("[+] UDP: Datagram recu! Port Source: ");
    serial_print_dec(src_port);
    serial_print(" -> Port Dest: ");
    serial_print_dec(dst_port);
    serial_print(" | Taille: ");
    serial_print_dec(ulen);
    serial_print(" octets\n");

    socket_dispatch_udp(dst_port, payload + sizeof(udp_header_t), ulen - sizeof(udp_header_t));

    if (dst_port == DHCP_CLIENT_PORT) {
        dhcp_handle_packet((const dhcp_packet_t *)(payload + sizeof(udp_header_t)), ulen - sizeof(udp_header_t));
    } else if (src_port == DNS_PORT) {
        dns_handle_packet(payload + sizeof(udp_header_t), ulen - sizeof(udp_header_t));
    }
}

/* ========================================================================= */
/*                              IPV4 & DISPATCH                              */
/* ========================================================================= */

void ipv4_handle_packet(const eth_header_t *eth, const uint8_t *pkt, uint16_t len) {
    if (len < sizeof(ipv4_header_t)) return;

    const ipv4_header_t *ip = (const ipv4_header_t *)pkt;
    if (ip->dest_ip != guest_ip && ip->dest_ip != 0xFFFFFFFF) return;

    uint8_t ihl = (ip->ihl_version & 0x0F) * 4;
    uint16_t total_len = ntohs(ip->total_len);
    if (total_len < ihl || total_len > len) return;

    uint16_t payload_len = total_len - ihl;
    const uint8_t *payload = pkt + ihl;

    if (ip->protocol == IP_PROTO_ICMP) {
        icmp_handle_packet(eth, ip, payload, payload_len);
    } else if (ip->protocol == IP_PROTO_UDP) {
        udp_handle_packet(eth, ip, payload, payload_len);
    }
}

void net_handle_packet(const uint8_t *pkt, uint16_t len) {
    if (len < sizeof(eth_header_t)) return;

    const eth_header_t *eth = (const eth_header_t *)pkt;
    uint16_t type = ntohs(eth->ethertype);

    if (type == ETH_TYPE_ARP) {
        if (len >= sizeof(eth_header_t) + sizeof(arp_packet_t)) {
            const arp_packet_t *arp = (const arp_packet_t *)(pkt + sizeof(eth_header_t));
            arp_handle_packet(eth, arp);
        }
    } else if (type == ETH_TYPE_IPV4) {
        if (len >= sizeof(eth_header_t) + sizeof(ipv4_header_t)) {
            ipv4_handle_packet(eth, pkt + sizeof(eth_header_t), len - sizeof(eth_header_t));
        }
    }
}
