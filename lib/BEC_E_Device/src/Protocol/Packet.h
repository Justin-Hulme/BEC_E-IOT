#pragma once

#include "src/BEC_E_Device.h"

uint8_t* read_packet(PacketHeader header);
void handle_bad_packet(PacketHeader header);