#pragma once

#include "BEC_E_Device.h"

#ifndef MAX_REGISTERED_COMMAND_NUM
#define MAX_REGISTERED_COMMAND_NUM 10
#endif

// // struct for receiving RGB colors
// struct Color {
//     uint8_t r;
//     uint8_t g;
//     uint8_t b;
// };

// // union that allows for arrays of any type
// union ArgValue {
//     bool        bool_val;
//     int8_t      int8_val;
//     int16_t     int16_val;
//     int32_t     int32_val;
//     uint8_t     uint8_val;
//     uint16_t    uint16_val;
//     uint32_t    uint32_val;
//     float       float_val;
//     Color       color_val;
//     const char* str_val;
// };

// namespace Argument{
//     enum arg_type : uint8_t{
//         BOOL    = 0,
//         INT8    = 1,
//         INT16   = 2,
//         INT32   = 3,
//         UINT8   = 4,
//         UINT16  = 5,
//         UINT32  = 6,
//         FLOAT   = 7,
//         COLOR   = 8,
//         STRING  = 9
//     };
// }

// // enum for the different types of commands
// enum command_type {
//     BUTTON = 0,        // no data passed. Shown as a button
//     SWITCH = 1,        // passes in true or false. Shown as a switch
//     SLIDER_UINT8 = 2,  // passes in an int within the range. Shown as a SLIDER_UINT8. Must add 2 int args to the command
//     COLOR = 3,         // passes in a color struct. Shown as a color picker
//     DROPDOWN = 4,      // passes in an int with a specific value. Shown as a dropdown Must add at least 1 char* arg to the command
//     STRING = 5,        // passes in a string. Shown as a single line input
//     HIDDEN = 6,        // no data passed. Not shown
//     STRONG_BUTTON = 7, // no data passed. Shown as a button but requires a confirmation
// };

// struct Command {
//     const char* name;                                   // the display name
//     uint16_t id;                                        // the unique ID. Will be sent by server to call function
//     command_type type;                                  // the type of command, used to determine what is shown on the webpage
//     ArgValue* additional_args;                          // additional arguments to be passed to the server
//     uint8_t additional_arg_num;                         // the number of additional arguments
//     void (*receive_command_function)(ArgValue*, uint8_t); // the function to be called with the response
// } __attribute__((packed));

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