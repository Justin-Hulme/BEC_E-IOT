#include <stdint.h>

#ifndef BEC_E_H
#define BEC_E_H

#ifndef ARG_BUFFER_SIZE
#define ARG_BUFFER_SIZE 10
#endif

#ifndef MAX_REGISTERED_COMMAND_NUM
#define MAX_REGISTERED_COMMAND_NUM 10
#endif

#ifndef MAX_LOOP_FUNCTION_NUM
#define MAX_LOOP_FUNCTION_NUM 10
#endif

#ifndef WIFI_TIMEOUT_SECONDS
#define WIFI_TIMEOUT_SECONDS 5
#endif

#ifndef SSID_SIZE
#define SSID_SIZE 33
#endif

#ifndef WIFI_PASSWORD_SIZE
#define WIFI_PASSWORD_SIZE 33
#endif

#ifndef SERVER_IP_SIZE
#define SERVER_IP_SIZE 16
#endif

#ifndef SERVER_PORT_TCP
#define SERVER_PORT_TCP 15000
#endif

#ifndef SERVER_PORT_UDP
#define SERVER_PORT_UDP 15001
#endif

#ifndef DEVICE_NAME
#define DEVICE_NAME "default"
#endif

#ifndef DEVICE_ID
#define DEVICE_ID "0000"
#endif

#ifndef USE_UDP
#define USE_UDP false
#endif

#define MAGIC 0xBECE
#define COMMAND_SET 0 // TODO: implement command_set to clear the magic flag incase you add things to the eeprom

#define EEPROM_MAGIC 0x42

// struct for recieving RGB colors
struct Color {
    uint8 r;
    uint8 g;
    uint8 b;
};

// union that allows for arrays of any type
union ArgValue {
    bool        bool_val;
    int         int_val;
    uint16      uint_val;
    float       float_val;
    Color       color_val;
    const char* str_val;
};

// enum for the different types of commands
enum command_type {
    VOID_C = 0,     // no data passed. Shown as a push button
    BOOLEAN_C = 1,  // passes in true or false. Shown as a switch
    SCALE_C = 2,    // passes in an int within the range. Shown as a slider
    COLOR_C = 3,    // passes in a color struct. Shown as a color picker
    DROPDOWN_C = 4, // passes in an int with a specific value. Shown as a dropdown
    STRING_C = 5    // passes in a string. Shown as a single line input
};

// enum for the types of messages used by built in functions
enum default_message_type : uint16_t {
    LOG_MESSAGE = 65535,
    SEND_COMMAND = 65534
};

// struct for commands
struct Command {
    const char* name;                   // the display name
    uint16_t id;                        // the unique ID. Will be sent by server to call function
    command_type type;                  // the type of command, used to determine what is shown on the webpage
    void (*recieve_command)(ArgValue*); // the function to be called with the responce
} __attribute__((packed));

// struct for packet headers. Packed so that it can easily be sent
struct PacketHeader {
    uint16_t magic;         // the magic bytes indicating it is a valid packet
    uint8 command_set;      // idicates what command set it is operating with, not currently used
    uint16_t type;          // the type of packet. Indicates what function gets called on the  server
    uint32 packet_id;       // unique id of the packet, used to get rid of duplicates
    uint16_t packet_num;    // the packet number. 0 for a single packet message
    uint16_t total_packets; // the total number of packets in the message. 1 for a single packet message
    uint16_t payload_len;   // the length of the packet payload
} __attribute__((packed)); 

// functions that should be available to users of the library
namespace BEC_E {
    void main_setup(); // sets up the wifi and the server connections
    void main_loop(); // manages the server and runs the user defined loop functions
    void register_command(struct Command); // adds a command to the user commands list
    void register_loop_function(void (*loop_function)(ArgValue*)); // adds a function to the user defined loop functions
    PacketHeader build_packet_header(uint16_t, uint16_t, uint16_t, uint16_t); // builds a packet header removing the need to worry about all fields
    void send_log(char *, uint16_t); // sends a log message to the server
    void send_TCP(PacketHeader, void*, uint16_t); // sends a packet over TCP
    void send_UDP(PacketHeader, void*, uint16_t); // sends a packet over UDP
    void safe_delay(unsigned long); // delays for the specified time but runs the main loop while waiting
}

#endif //BEC_E_H