#include "RF24.h"
#include "Packet.hpp"
#include "DHCP.hpp"

#define _DHCP_SERVER_IP  {192, 168, 1, 1}
#define BROADCAST_IP    {255, 255, 255, 255}

constexpr uint8_t _dhcp_server_ip[4] = _DHCP_SERVER_IP;
constexpr uint8_t broadcast_ip[4] = BROADCAST_IP;

//#define DEBUG
#ifdef DEBUG
#define debug_print(data) Serial.print(data);
#define debug_println(data) Serial.println(data);
#define debug_printhex(data) Serial.print(data, HEX);
#else
#define debug_print(data)
#define debug_println(data)
#define debug_printhex(data)
#endif

RF24 radio(7, 8);

byte addresses[][6] = {
  "Enzzo",
  "Hazae",
};

uint8_t radio_number = 0;

void setup() {
    Serial.begin(115200);

    radio.begin();

    // Set the PA Level low to prevent power supply related issues since this is a
    // getting_started sketch, and the likelihood of close proximity of the devices. RF24_PA_MAX is default.
    radio.setPALevel(RF24_PA_LOW);

    radio.openWritingPipe(addresses[radio_number]);
    radio.openReadingPipe(1, addresses[!radio_number]);

    radio.startListening();
}

template<uint16_t PacketSize>
class UDPHelper {
private:
    IPv4Packet<PacketSize> ip_packet;
    UDPPacket udp_packet;

public:
    uint8_t own_ip[4] = {0, 0, 0, 0};

    bool send_data(
        byte* data,
        size_t data_size,
        uint16_t source_port,
        uint16_t destination_port,
        uint8_t (&&destination_ip)[4]
    ) {
        bool result;

        debug_print(F("Data to send is "));
        for (size_t i = 0; i < data_size; i++) {
            debug_printhex(data[i]);
            debug_print(' ');
        }
        debug_print('\n');
        
        udp_packet.fill_packet(source_port, destination_port, (uint8_t*)data, data_size);

        uint16_t ip_payload_size = udp_packet.get_length();

        debug_print(F("Payload to send is "));
        for (size_t i = 0; i < ip_payload_size; i++) {
            debug_printhex(udp_packet[i]);
            debug_print(' ');
        }
        debug_print('\n');
        
        debug_print(F("IP payload size is "));
        debug_println(ip_payload_size);

        IPv4Header& ipv4_header = ip_packet.get_header();
        ipv4_header.flag_offset = 0;
        size_t index = 0;
        while (index < ip_payload_size) {
            size_t fragment_size;
            bool more_fragments;

            if ((ip_payload_size - index) > IPv4Packet<PacketSize>::PAYLOAD_SIZE) {
                fragment_size = IPv4Packet<PacketSize>::PAYLOAD_SIZE;
                more_fragments = true;
            } else {
                fragment_size = ip_payload_size - index;
                more_fragments = false;
            }

            byte fragment_payload[IPv4Packet<PacketSize>::PAYLOAD_SIZE];
            for (size_t i = 0; i < fragment_size; i++) {
                fragment_payload[i] = udp_packet[index + i];
            }
            ip_packet.fill_packet(
                10,
                (uint8_t(&&)[4])own_ip,
                (uint8_t(&&)[4])destination_ip,
                fragment_payload,
                fragment_size,
                more_fragments
            );

            debug_print(F("Now sending index "));
            debug_print(index);
            debug_print(F(" with size "));
            debug_println(fragment_size);
            debug_println(F("Data to send:"));
            for (size_t i = 0; i < fragment_size; i++) {
                debug_printhex(fragment_payload[i]);
                debug_print(' ');
            }
            debug_println('\n');
            index += fragment_size;
            radio.stopListening();
            
            while (radio.available()) {
                byte temp[32];
                radio.read(temp, 32);
            }

            result = radio.write(&ip_packet, ip_packet.get_length());
            if (!result) {
                break;
            }

            radio.startListening();

            IPv4Header& ipv4_header = ip_packet.get_header();
            ipv4_header.flag_offset += fragment_size / 8;
        }

        return result;
    }

    size_t receive_data(byte* buf, size_t buffer_size, uint16_t port) {
        if (radio.available()) {
            debug_println(F("radio.available"));
            bool done = false;
            bool found_first = false;
            size_t received = 0;
            
            udp_packet.set_buffer(buf);

            while (!done) {
                
                uint32_t timeout_start = millis();
                while (!radio.available()) {
                    if (millis() - timeout_start >= 1000) {
                        debug_println(F("Timeout"));
                        return received;
                    }
                }

                radio.read(&ip_packet, PacketSize);

                if (!ip_packet.get_header().is_checksum_valid()) {
                    continue;
                }

                debug_print(F("Received packet for IP "));
                for (byte i = 0; i < 4; i++) {
                    debug_print(ip_packet.get_header().destination[i]);
                    debug_print(' ');
                }
                debug_print('\n');

                debug_print(F("Fragment offset: "));
                debug_println(ip_packet.get_header().flag_offset);    

                if (ip_packet.is_for_me((uint8_t(&&)[4])own_ip) || ip_packet.is_for_me(BROADCAST_IP)) {
                    const IPv4Header& ipv4_header = ip_packet.get_header();

                    debug_println("Checking for first packet");

                    if (ipv4_header.flag_offset == 0) {
                        received = 0;
                        found_first = true;
                    }
                    
                    if (found_first == false) {
                        continue;
                    }

                    byte* ipv4_payload = ip_packet.get_payload();
                    size_t fragment_size = ipv4_header.length - sizeof(IPv4Header);

                    debug_print(F("Now receiving index "));
                    debug_print(received);
                    debug_print(F(" with size "));
                    debug_println(fragment_size);

                    for (size_t i = 0; i < fragment_size; i++) {
                        udp_packet[received] = ipv4_payload[i];
                        received++;
                    }

                    if (!(ipv4_header.flags & (1 << 2))) {
                        done = true;
                    }
                }
            }
            if (udp_packet.get_destination_port() != port) {
                return 0;
            }
            return received - sizeof(UDPHeader);
        }
        return 0;
    }
};

enum class ClientState {
    IDLE,
    DHCP_DISCOVERY,
    DHCP_WAIT_OFFER,
    DHCP_REQUEST,
    DHCP_WAIT_ACK,
};

enum class DCHPState {
    IDLE,
    DHCP_OFFER,
    DHCP_ACK,
    DHCP_NACK,
};

void loop0() {
    UDPHelper<32> udp_helper;
    byte buf[500];

    uint8_t temp_ip[4];
    uint8_t dhcp_server_ip[4];

    bool has_ip = false;
    uint32_t lease_start = 0;
    uint32_t lease_time = 0;
    ClientState state = ClientState::DHCP_DISCOVERY;

    uint32_t last_time = millis();

    while (1) {
        uint32_t current_time = millis();
        

        if (has_ip == false) {
            if (current_time - last_time >= 10000) {
                last_time = current_time;
                state = ClientState::DHCP_DISCOVERY;
            }
        } else {
            if (current_time - lease_start >= lease_time) {
                Serial.println("Lease time expired. IP address has been lost.");
                has_ip = false;
            }
            if (current_time - last_time >= lease_time / 2) {
                last_time = current_time;
                state = ClientState::DHCP_REQUEST;
            }
        }

        switch (state) {
            case ClientState::DHCP_DISCOVERY: {
                DHCPDiscover dhcp_discover;
                dhcp_discover.ip_request[2] = 192;
                dhcp_discover.ip_request[3] = 168;
                dhcp_discover.ip_request[4] = 1;
                dhcp_discover.ip_request[5] = 2;
                if (udp_helper.send_data(
                    (byte*)&dhcp_discover,
                    sizeof(DHCPDiscover),
                    68,
                    67,
                    BROADCAST_IP)
                ) {
                    state = ClientState::DHCP_WAIT_OFFER;
                } else {
                    Serial.println("Could not DHCPDiscover");
                }
                break;
            }
            case ClientState::DHCP_WAIT_OFFER: {
                size_t received_size = udp_helper.receive_data(buf, 500, 68);
                if (received_size == sizeof(DHCPOffer)) {
                    const DHCPOffer& dhcp_offer = *(DHCPOffer*)&buf[0];
                    if (dhcp_offer.magic_cookie == 0x63825363) {
                        Serial.print("Got DHCPOffer ");
                        for (uint8_t i = 0; i < 4; i++) {
                            temp_ip[i] = dhcp_offer.yiaddr[i];
                            Serial.print(dhcp_offer.yiaddr[i]);
                            Serial.print(' ');
                        }
                        Serial.print("from ");
                        for (uint8_t i = 0; i < 4; i++) {
                            dhcp_server_ip[i] = dhcp_offer.siaddr[i];
                            Serial.print(dhcp_offer.siaddr[i]);
                            Serial.print(' ');
                        }
                        Serial.print('\n');
                        state = ClientState::DHCP_REQUEST;
                    } else {
                        Serial.println("Wrong magic cookie");
                        state = ClientState::IDLE;
                    }
                }
                break;
            }
            case ClientState::DHCP_REQUEST: {
                DHCPRequest dhcp_request;
                for (uint8_t i = 0; i < 4; i++) {
                    dhcp_request.siaddr[i] = dhcp_server_ip[i];
                    dhcp_request.requested_ip[i + 2] = temp_ip[i];
                    dhcp_request.dhcp_server[i + 2] = dhcp_server_ip[i];
                }
                if (udp_helper.send_data(
                    (byte*)&dhcp_request,
                    sizeof(DHCPRequest),
                    68,
                    67,
                    BROADCAST_IP)
                ) {
                    state = ClientState::DHCP_WAIT_ACK;
                } else {
                    Serial.println("Could not DHCPRequest");
                }
                break;
            }
            case ClientState::DHCP_WAIT_ACK: {
                size_t received_size = udp_helper.receive_data(buf, 500, 68);
                if (received_size == sizeof(DHCPAck)) {
                    const DHCPAck& dhcp_ack = *(DHCPAck*)&buf[0];
                    if (dhcp_ack.magic_cookie == 0x63825363) {
                        Serial.print("Got DHCPAck with IP ");
                        for (uint8_t i = 0; i < 4; i++) {
                            udp_helper.own_ip[i] = dhcp_ack.yiaddr[i];
                            Serial.print(dhcp_ack.yiaddr[i]);
                            Serial.print(' ');
                        }
                        Serial.print("from ");
                        for (uint8_t i = 0; i < 4; i++) {
                            Serial.print(dhcp_ack.siaddr[i]);
                            Serial.print(' ');
                        }
                        Serial.print('\n');
                        lease_time =
                            ((uint32_t)dhcp_ack.ip_lease_time[2] << 24) |
                            ((uint32_t)dhcp_ack.ip_lease_time[3] << 16) |
                            ((uint32_t)dhcp_ack.ip_lease_time[4] << 8) |
                            ((uint32_t)dhcp_ack.ip_lease_time[5] << 0);
                        lease_time *= 1000;
                        lease_start = millis();
                        Serial.print("Lease time: ");
                        Serial.println(lease_time);
                        has_ip = true;
                        state = ClientState::IDLE;
                    } else {
                        Serial.println("Wrong magic cookie");
                        state = ClientState::IDLE;
                    }
                }
                break;
            }
            case ClientState::IDLE: {
                break;
            }
        }
    }
}

void loop1() {
    UDPHelper<32> udp_helper;
    byte buf[500];

    struct IPSlot {
        uint8_t ip_addr[4];
        bool taken = false;
        uint32_t lease_time;
    };

    IPSlot slots[3];

    uint32_t last_time = millis();
    while (1) {
        uint32_t current_time = millis();
        uint32_t delta_time = current_time - last_time;
        last_time = current_time;
        for (uint8_t i = 0; i < 3; i++) {
            if (slots[i].taken) {
                if (slots[i].lease_time > delta_time) {
                    slots[i].lease_time -= delta_time;
                } else {
                    slots[i].taken = false;
                    Serial.print("IPSlot "); Serial.print(i + 2);
                    Serial.println(" has been released for inactivity.");
                }
            }
        }

        size_t received_size = udp_helper.receive_data(buf, 500, 67);
        if (received_size) {
            if (received_size >= sizeof(DHCPBase)) {
                const DHCPBase& dhcp_base = *(DHCPBase*)&buf[0];
                if (dhcp_base.magic_cookie == 0x63825363) {
                    switch (dhcp_base.dhcp_task[2]) {
                        case 1: {
                            DHCPDiscover& dhcp_discover = *(DHCPDiscover*)&buf[0];

                            if (dhcp_discover.ip_request[2] != _dhcp_server_ip[0] ||
                                dhcp_discover.ip_request[3] != _dhcp_server_ip[1] ||
                                dhcp_discover.ip_request[4] != _dhcp_server_ip[2]) {
                                Serial.println("Invalid IP request");
                                break;
                            }

                            bool found_offer = false;
                            if (dhcp_discover.ip_request[5] < 2 && dhcp_discover.ip_request[5] > 4) {
                                Serial.println("There is no IP slot to offer at the requested IP");
                                for (uint8_t i = 0; i < 3; i++) {
                                    if (!slots[i].taken) {
                                        Serial.print("Offering slot "); Serial.println(i + 2);
                                        dhcp_discover.ip_request[5] = i + 2;
                                        found_offer = true;
                                        break;
                                    }
                                }
                            } else {
                                uint8_t slot_index = dhcp_discover.ip_request[5] - 2;
                                if (slots[slot_index].taken) {
                                    Serial.println("Requested IP is already taken");
                                    for (uint8_t i = 0; i < 3; i++) {
                                        if (!slots[i].taken) {
                                            Serial.print("Offering slot "); Serial.println(i + 2);
                                            dhcp_discover.ip_request[5] = i + 2;
                                            found_offer = true;
                                            break;
                                        }
                                    }
                                } else {
                                    found_offer = true;
                                }
                            }

                            if (found_offer == false) {
                                Serial.println("There is no IP slot to offer");
                                break;
                            }
                            
                            DHCPOffer dhcp_offer;
                            for (uint8_t i = 0; i < 4; i++) {
                                dhcp_offer.yiaddr[i] = dhcp_discover.ip_request[i+2];
                                dhcp_offer.siaddr[i] = _dhcp_server_ip[i];
                                dhcp_offer.dhcp_server[i+2] = _dhcp_server_ip[i];
                            }
                            if (!udp_helper.send_data(
                                (byte*)&dhcp_offer,
                                sizeof(DHCPOffer),
                                67,
                                68,
                                BROADCAST_IP)
                            ) {
                                Serial.println("Could not DHCPOffer");
                            }
                            break;
                        }
                        case 3: {
                            const DHCPRequest& dhcp_request = *(DHCPRequest*)&buf[0];

                            if (dhcp_request.requested_ip[2] != _dhcp_server_ip[0] ||
                                dhcp_request.requested_ip[3] != _dhcp_server_ip[1]||
                                dhcp_request.requested_ip[4] != _dhcp_server_ip[2]) {
                                Serial.println("Invalid IP request");
                                break;
                            }
                            
                            if (dhcp_request.requested_ip[5] < 2 && dhcp_request.requested_ip[5] > 4) {
                                Serial.println("There is no IP slot to offer at the requested IP");
                                break;
                            }

                            uint8_t slot_index = dhcp_request.requested_ip[5] - 2;
                            if (slots[slot_index].taken) {
                                for (uint8_t i = 0; i < 4; i++) {
                                    if (slots[slot_index].ip_addr[i] != dhcp_request.requested_ip[i + 2]) {
                                        Serial.println("Requested IP is already taken");
                                        break;
                                    }
                                }
                            }
                            
                            DHCPAck dhcp_ack;                            
                            for (uint8_t i = 0; i < 4; i++) {
                                slots[slot_index].ip_addr[i] = dhcp_request.requested_ip[i+2];
                                dhcp_ack.yiaddr[i] = dhcp_request.requested_ip[i+2];
                                dhcp_ack.siaddr[i] = _dhcp_server_ip[i];
                                dhcp_ack.dhcp_server[i+2] = _dhcp_server_ip[i];
                            }
                            if (!udp_helper.send_data(
                                (byte*)&dhcp_ack,
                                sizeof(DHCPAck),
                                67,
                                68,
                                BROADCAST_IP)
                            ) {
                                Serial.println("Could not DHCPAck");
                            }
                            slots[slot_index].lease_time = 16000;
                            if (slots[slot_index].taken == false) {
                                slots[slot_index].taken = true;
                                Serial.print("IPSlot "); Serial.print(slot_index + 2);
                                Serial.println(" has been taken.");
                            }
                            break;
                        }
                    }
                }
            }
        }
    }
}

void loop() {
    if (!radio_number) {
        Serial.println(F("Entering loop1"));
        loop1();
    } else {
        Serial.println(F("Entering loop0"));
        loop0();
    }
}
