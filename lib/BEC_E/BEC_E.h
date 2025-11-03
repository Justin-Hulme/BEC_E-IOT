#include <stdint.h>

#ifndef BEC_E_H
#define BEC_E_H

// ----------- Buffer Config -----------

#ifndef ARG_BUFFER_SIZE
#define ARG_BUFFER_SIZE 10
#endif

#ifndef MAX_REGISTERED_COMMAND_NUM
#define MAX_REGISTERED_COMMAND_NUM 10
#endif

#ifndef MAX_LOOP_FUNCTION_NUM
#define MAX_LOOP_FUNCTION_NUM 10
#endif

#ifndef MAX_COMMAND_ADDITIONAL_ARGS
#define MAX_COMMAND_ADDITIONAL_ARGS 5
#endif

// ------------ WIFI Config ------------

#ifndef WIFI_TIMEOUT_SECONDS
#define WIFI_TIMEOUT_SECONDS 5
#endif

#ifndef SSID_SIZE
#define SSID_SIZE 33
#endif

#ifndef WIFI_PASSWORD_SIZE
#define WIFI_PASSWORD_SIZE 33
#endif

// ----------- Server Config -----------

#ifndef SERVER_IP_SIZE
#define SERVER_IP_SIZE 16
#endif

#ifndef SERVER_PORT_TCP
#define SERVER_PORT_TCP 15000
#endif

#ifndef SERVER_PORT_UDP
#define SERVER_PORT_UDP 15001
#endif

#ifndef TCP_CONNECTION_ATTEMPTS
#define TCP_CONNECTION_ATTEMPTS 10
#endif

// ----------- Device Config -----------

#ifndef DEVICE_NAME
#define DEVICE_NAME "default"
#endif

#ifndef DEVICE_ID
#define DEVICE_ID "0000"
#endif

#ifndef USE_UDP
#define USE_UDP false
#endif

#ifndef CURRENT_VERSION
#define CURRENT_VERSION "0.0.0"
#endif

// ---------- Internal Macros ----------

#define MAGIC 0xBECE
#define COMMAND_SET 0

#define EEPROM_MAGIC 0x42
#define EEPROM_VERSION 0

// struct for receiving RGB colors
struct Color {
    uint8 r;
    uint8 g;
    uint8 b;
};

// union that allows for arrays of any type
union ArgValue {
    bool        bool_val;
    int8_t      int8_val;
    int16_t     int16_val;
    int32_t     int32_val;
    uint8       uint8_val;
    uint16      uint16_val;
    uint32      uint32_val;
    float       float_val;
    Color       color_val;
    const char* str_val;
};

namespace Argument{
    enum arg_type : uint8_t{
        BOOL    = 0,
        INT8    = 1,
        INT16   = 2,
        INT32   = 3,
        UINT8   = 4,
        UINT16  = 5,
        UINT32  = 6,
        FLOAT   = 7,
        COLOR   = 8,
        STRING  = 9
    };
}

// enum for the different types of commands
enum command_type {
    BUTTON = 0,        // no data passed. Shown as a button
    SWITCH = 1,        // passes in true or false. Shown as a switch
    SLIDER_UINT8 = 2,  // passes in an int within the range. Shown as a SLIDER_UINT8. Must add 2 int args to the command
    COLOR = 3,         // passes in a color struct. Shown as a color picker
    DROPDOWN = 4,      // passes in an int with a specific value. Shown as a dropdown Must add at least 1 char* arg to the command
    STRING = 5,        // passes in a string. Shown as a single line input
    HIDDEN = 6,        // no data passed. Not shown
    STRONG_BUTTON = 7, // no data passed. Shown as a button but requires a confirmation
};

// enum for the types of messages used by built in functions
enum default_message_type : uint16_t {
    LOG_MESSAGE     = 65535,
    SEND_COMMAND    = 65534,
    SEND_NAME       = 65533,
    ESTABLISH_UDP   = 65532,
    RESEND          = 65531,
};

// struct for commands
struct Command {
    const char* name;                                   // the display name
    uint16_t id;                                        // the unique ID. Will be sent by server to call function
    command_type type;                                  // the type of command, used to determine what is shown on the webpage
    ArgValue* additional_args;                          // additional arguments to be passed to the server
    uint8_t additional_arg_num;                         // the number of additional arguments
    void (*receive_command_function)(ArgValue*, uint8); // the function to be called with the response
} __attribute__((packed));

// struct for packet headers. Packed so that it can easily be sent
struct PacketHeader {
    uint16_t magic;           // the magic bytes indicating it is a valid packet
    uint8 command_set;        // indicates what command set it is operating with, not currently used
    uint16_t type;            // the type of packet. Indicates what function gets called on the  server
    uint32 packet_id;         // unique id of the packet, used to get rid of duplicates
    uint16_t packet_num;      // the packet number. 0 for a single packet message
    uint16_t total_packets;   // the total number of packets in the message. 1 for a single packet message
    uint16_t payload_len;     // the length of the packet payload
    uint8_t argument_number;  // the number of arguments in the payload
} __attribute__((packed)); 

// functions that should be available to users of the library
namespace BEC_E {
    void main_setup(); // sets up the wifi and the server connections
    void main_loop(); // manages the server and runs the user defined loop functions
    void register_command(struct Command); // adds a command to the user commands list
    void register_loop_function(void (*loop_function)(ArgValue*)); // adds a function to the user defined loop functions
    PacketHeader build_packet_header(uint16_t, uint16_t, uint16_t, uint16_t, uint8_t); // builds a packet header removing the need to worry about all fields
    void send_log(const char *); // sends a log message to the server
    void send_TCP(PacketHeader, void*); // sends a packet over TCP
    void send_UDP(PacketHeader, void*); // sends a packet over UDP
    void safe_delay(unsigned long); // delays for the specified time but runs the main loop while waiting
}

#endif //BEC_E_H