#include "Packet.h"

void handle_bad_packet(PacketHeader header){
    // build the buffer and specify the type
    uint8_t buffer[1 + sizeof(uint32_t)];
    buffer[0] = Argument::UINT32;

    // copy the packed id into the buffer
    memcpy(buffer + 1, &header.packet_id, sizeof(uint32_t));

    // build the header
    PacketHeader resend_header = BEC_E::build_packet_header(RESEND, 0, 1, sizeof(buffer), 1);

    // request the server to resend the packet
    BEC_E::send_TCP(resend_header, buffer);
}