#include "stdint.h"

static uint16_t own_src_port = 80;
static uint8_t own_ip[4] = {192, 168, 0, 112};

struct IPv4Header {
    union {
        struct {
            uint8_t  version     : 4;
            uint8_t  IHL         : 4;
            uint8_t  DSCP        : 6;
            uint8_t  ECN         : 2;
            uint16_t length;
            uint16_t id;
            uint8_t  flags       : 3;
            uint16_t flag_offset : 13;
            uint8_t  time_to_live;
            uint8_t  protocol;
            uint16_t checksum;
            uint8_t  source[4];
            uint8_t  destination[4];
        };
        uint16_t header_data[10];
    };

    uint16_t compute_checksum() const {
        uint16_t accumulator = 0;
        
        for (const uint16_t data : header_data) {
            uint16_t new_value = accumulator + data;
            if (new_value < accumulator) {
                new_value++;
            }
            accumulator = new_value;
        }

        return ~accumulator;
    }

    void assign_checksum() {
        checksum = 0;
        checksum = compute_checksum();
    }

    bool is_checksum_valid() const {
        return !compute_checksum();
    }
};

struct UDPHeader {
    uint16_t source_port;
    uint16_t destination_port;
    uint16_t length;
    uint16_t checksum;
};

template<uint16_t PacketSize>
class IPv4Packet {
private:
    constexpr static size_t BASE_PAYLOAD_SIZE = PacketSize - sizeof(IPv4Header);
    
public:
    constexpr static size_t PAYLOAD_SIZE = BASE_PAYLOAD_SIZE - BASE_PAYLOAD_SIZE % 8;

private:
    IPv4Header ipv4_header = {};
    byte payload[PAYLOAD_SIZE];

    uint16_t id_counter = 0;

public:
    IPv4Packet() {
        ipv4_header.version = 4;
        ipv4_header.IHL = 5;
        ipv4_header.DSCP = 0;
        ipv4_header.ECN = 0;
        ipv4_header.protocol = 17;
    }

    void fill_packet(
        uint8_t time_to_live,
        uint8_t (&&source_addr)[4],
        uint8_t (&&destination_addr)[4],
        uint8_t* data,
        size_t data_size,
        bool more_fragments
    ) {
        ipv4_header.time_to_live = time_to_live;
        for (uint8_t i = 0; i < 4; i++) {
            ipv4_header.source[i] = source_addr[i];
            ipv4_header.destination[i] = destination_addr[i];
        }

        ipv4_header.length = sizeof(IPv4Header) + data_size;
        ipv4_header.id = id_counter++;
        ipv4_header.flags = (more_fragments << 2);        

        ipv4_header.assign_checksum();
        
        for (uint8_t i = 0; i < data_size; i++) {
            payload[i] = data[i];
        }
    }

    uint16_t get_length() const {
        return ipv4_header.length;
    }

    bool get_flags() const {
        return ipv4_header.flags;
    }

    bool get_flag_offset() const {
        return ipv4_header.flag_offset;
    }

    byte* get_payload() const {
        return payload;
    }

    const IPv4Header& get_header() const {
        return ipv4_header;
    }

    IPv4Header& get_header() {
        return ipv4_header;
    }

    bool is_for_me(uint8_t (&&addr)[4]) const {
        for (uint8_t i = 0; i < 4; i++) {
            if (ipv4_header.destination[i] != addr[i]) {
                return false;
            }
        }
        return true;
    }
};

class UDPPacket {
private:
    UDPHeader udp_header = {};
    byte* payload = nullptr;

public:
    byte& operator[](size_t index) {
        if (index < sizeof(UDPHeader)) {
            return ((byte*)&udp_header)[index];
        } else if (index < udp_header.length) {
            return payload[index - sizeof(UDPHeader)];
        }

        Serial.println("Out of bounds exception.");
        while(1);
    }

    void fill_packet(
        uint16_t source_port,
        uint16_t destination_port,
        uint8_t* data,
        size_t data_size
    ) {
        udp_header.source_port = source_port;
        udp_header.destination_port = destination_port;

        uint16_t ipv4_payload_size = sizeof(UDPHeader) + data_size;
        udp_header.length = ipv4_payload_size;

        udp_header.checksum = 0;

        payload = data;
    }

    uint16_t get_length() {
        return udp_header.length;
    }

    void set_buffer(byte* buffer) {
        payload = buffer;
    }

    uint16_t get_destination_port() {
        return udp_header.destination_port;
    }
};

