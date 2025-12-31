#pragma once

struct PacketHeader;

uint8_t* read_packet(PacketHeader header);
void handle_bad_packet(PacketHeader header);