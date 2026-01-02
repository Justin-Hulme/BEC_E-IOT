#pragma once

#include <WiFiClient.h>
#include <WiFiUdp.h>

#include "BEC_E_Device.h"

#ifndef WIFI_TIMEOUT_SECONDS
#define WIFI_TIMEOUT_SECONDS 20
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

#ifndef TCP_CONNECTION_ATTEMPTS
#define TCP_CONNECTION_ATTEMPTS 10
#endif

extern char server_ip[];
extern char ssid[];
extern char password[];

extern WiFiClient tcp_client;
extern WiFiUDP udp_client;

// enum for the types of messages used by built in functions
enum default_message_type : uint16_t {
    LOG_MESSAGE     = 65535,
    SEND_COMMAND    = 65534,
    SEND_NAME       = 65533,
    ESTABLISH_UDP   = 65532,
    RESEND          = 65531,
};

// function prototypes for internal functions
void send_single_command(const Command&);
bool connect_wifi(char*, char*);
void run_AP();
uint16_t calculate_crc16(const uint8_t* data, size_t length);
bool validate_crc(uint8_t* buffer, PacketHeader header);
uint16_t parse_argument(ArgValue& arg, uint8_t* payload);