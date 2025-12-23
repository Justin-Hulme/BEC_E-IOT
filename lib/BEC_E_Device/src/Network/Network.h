#pragma once

#include "src/BEC_E_Device.h"

extern char server_ip[];

// function prototypes for internal functions
void send_single_command(const Command&);
bool connect_wifi(char*, char*);
void run_AP();
uint16_t calculate_crc16(const uint8_t* data, size_t length);
bool validate_crc(uint8_t* buffer, PacketHeader header);
uint16_t parse_argument(ArgValue& arg, uint8_t* payload);