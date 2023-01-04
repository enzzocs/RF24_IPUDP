#include "stdint.h"

struct DHCPDiscover {
    uint8_t op = 0x01;
    uint8_t htype = 0x01;
    uint8_t hlen = 0x06;
    uint8_t hops = 0x00;
    uint32_t xid = 0x3903F326;
    uint16_t secs = 0x0000;
    uint16_t flags = 0x0000;
    uint8_t ciaddr[4] = {0, 0, 0, 0};
    uint8_t yiaddr[4] = {0, 0, 0, 0};
    uint8_t siaddr[4] = {0, 0, 0, 0};
    uint8_t giaddr[4] = {0, 0, 0, 0};
    uint8_t chaddr[4][4];
    uint8_t server_name[64];
    uint8_t filename[128];
    uint32_t magic_cookie = 0x63825363;
    uint8_t dhcp_task[3] = {53, 1, 1};
    uint8_t ip_request[6] = {50, 4};
    uint8_t parameter_req_list[6] = {55, 4, 1, 3, 15, 6};
    uint8_t endmark = 0xFF;
};

struct DHCPOffer {
    uint8_t op = 0x02;
    uint8_t htype = 0x01;
    uint8_t hlen = 0x06;
    uint8_t hops = 0x00;
    uint32_t xid = 0x3903F326;
    uint16_t secs = 0x0000;
    uint16_t flags = 0x0000;
    uint8_t ciaddr[4];
    uint8_t yiaddr[4];
    uint8_t siaddr[4];
    uint8_t giaddr[4];
    uint8_t chaddr[4][4];
    uint8_t server_name[64];
    uint8_t filename[128];
    uint32_t magic_cookie = 0x63825363;
    uint8_t dhcp_task[3] = {53, 1, 2};
    uint8_t subnet_mask[6] = {1, 4};
    uint8_t router_ip[6] = {3, 4};
    uint8_t ip_lease_time[6] = {51, 4, 0x00, 0x00, 0x00, 0x10};
    uint8_t dhcp_server[6] = {54, 4};
    uint8_t dns_servers[6] = {6, 4, 0, 0, 0, 0};
};

struct DHCPRequest {
    uint8_t op = 0x01;
    uint8_t htype = 0x01;
    uint8_t hlen = 0x06;
    uint8_t hops = 0x00;
    uint32_t xid = 0x3903F326;
    uint16_t secs = 0x0000;
    uint16_t flags = 0x0000;
    uint8_t ciaddr[4];
    uint8_t yiaddr[4];
    uint8_t siaddr[4];
    uint8_t giaddr[4];
    uint8_t chaddr[4][4];
    uint8_t server_name[64];
    uint8_t filename[128];
    uint32_t magic_cookie = 0x63825363;
    uint8_t dhcp_task[3] = {53, 1, 3};
    uint8_t requested_ip[6] = {50, 4};
    uint8_t dhcp_server[6] = {54, 4};
};

struct DHCPAck {
    uint8_t op = 0x02;
    uint8_t htype = 0x01;
    uint8_t hlen = 0x06;
    uint8_t hops = 0x00;
    uint32_t xid = 0x3903F326;
    uint16_t secs = 0x0000;
    uint16_t flags = 0x0000;
    uint8_t ciaddr[4];
    uint8_t yiaddr[4];
    uint8_t siaddr[4];
    uint8_t giaddr[4];
    uint8_t chaddr[4][4];
    uint8_t server_name[64];
    uint8_t filename[128];
    uint32_t magic_cookie = 0x63825363;
    uint8_t dhcp_task[3] = {53, 1, 5};
    uint8_t subnet_mask[6] = {1, 4};
    uint8_t router_ip[6] = {3, 4};
    uint8_t ip_lease_time[6] = {51, 4, 0x00, 0x00, 0x00, 0x10};
    uint8_t dhcp_server[6] = {54, 4};
    uint8_t dns_servers[6] = {6, 4, 0, 0, 0, 0};
};

struct DHCPBase {
    uint8_t op;
    uint8_t htype;
    uint8_t hlen;
    uint8_t hops;
    uint32_t xid;
    uint16_t secs;
    uint16_t flags;
    uint8_t ciaddr[4];
    uint8_t yiaddr[4];
    uint8_t siaddr[4];
    uint8_t giaddr[4];
    uint8_t chaddr[4][4];
    uint8_t server_name[64];
    uint8_t filename[128];
    uint32_t magic_cookie;
    uint8_t dhcp_task[3];
};