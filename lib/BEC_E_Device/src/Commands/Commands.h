#pragma once

#include "src/BEC_E_Device.h"

// make globals available to everyone
extern Command built_in_commands[];
extern Command registered_commands [];

// function prototypes for build in commands
void handle_restart(ArgValue *, uint8_t);
void handle_update(ArgValue *, uint8_t);
void handle_send_commands(ArgValue *, uint8_t);
void handle_send_name(ArgValue *, uint8_t);
void handle_factory_reset(ArgValue *, uint8_t);

// function prototypes for internal functions
bool handle_command(PacketHeader header, uint8_t* buffer);
bool check_command(const Command& command, const PacketHeader& header, uint8_t* buffer);
void init_registered_commands();
uint16_t parse_argument(ArgValue& arg, uint8_t* payload);