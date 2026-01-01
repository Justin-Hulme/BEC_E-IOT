#include "BEC_E_Device.h"

#include <EEPROM.h>
#include <Arduino.h>

#include "EEPROM/BEC_E_EEPROM.h"
#include "Network/Network.h"
#include "Commands/Commands.h"
#include "Packet/Packet.h"
#include "Areana/Arena.h"

// array of loop functions defaulting to a null function
void (*loop_functions[MAX_LOOP_FUNCTION_NUM])() = {nullptr};

// function prototypes
void run_loop_functions();
uint8_t* read_in_packet(PacketHeader header);

namespace BEC_E {
    void main_setup(){
        Serial.begin(115200);
        EEPROM.begin(1 + SSID_SIZE + WIFI_PASSWORD_SIZE + SERVER_IP_SIZE);

        // check for magic to see if the eeprom has been reset
        if (EEPROM.read(0) != EEPROM_MAGIC){
            run_AP();
        }

        // clear the EEPROM if there is a new EEPROM version we have to follow
        if (EEPROM.read(1) != EEPROM_VERSION){
            clear_EEPROM();

            // record the new EEPROM version
            EEPROM.write(1, EEPROM_VERSION);

            // resetup wifi
            run_AP();
        }

        // read in the ssid
        for (int i = 0; i < SSID_SIZE; i++){
            ssid[i] = EEPROM.read(2 + i);
        }

        // read in the password
        for (int i = 0; i < WIFI_PASSWORD_SIZE; i++){
            password[i] = EEPROM.read(2 + SSID_SIZE + i);
        }

        // read in the server ip
        for (int i = 0; i < SERVER_IP_SIZE; i++){
            server_ip[i] = EEPROM.read(2 + SSID_SIZE + WIFI_PASSWORD_SIZE + i);
        }

        // try to connect to wifi and set up AP if the saved values fail
        if (! connect_wifi(ssid, password)){
            run_AP();
        }

        int attempts = 0;

        // attempt to connect to TCP
        while (!tcp_client.connect(server_ip, SERVER_PORT_TCP)){
            attempts ++;
            delay(1000);

            // if we try and connect too many time then run the ap again incase the ip was wrong
            if (attempts >= TCP_CONNECTION_ATTEMPTS){
                run_AP();
            }
        }

        BEC_E::send_log(DEVICE_NAME "_" DEVICE_ID " CONNECTED");

        // connect to udp
        if (USE_UDP){
            udp_client.begin(SERVER_PORT_UDP);

            // get the port
            uint16_t port = SERVER_PORT_UDP;

            // build buffer and specify the data type
            uint8_t buffer[1 + sizeof(uint16_t)];
            buffer[0] = Argument::UINT16;

            // copy the port into the buffer
            memcpy(buffer + 1, &port, sizeof(uint16_t));

            // build the header
            PacketHeader header = build_packet_header(ESTABLISH_UDP, 0, 1, sizeof(buffer), 1);
            
            // tell the server to start listening to UDP
            send_TCP(header, buffer);
        }

        init_registered_commands();
    }

    void main_loop(){
        run_loop_functions();

        // TODO: implement heartbeat packet
        
        // skip packet stuff if we don't even have a client conencted
        if (!tcp_client.connected() || !tcp_client.available()) return;
        
        // read in header
        PacketHeader header;
        if (tcp_client.readBytes((char*)&header, sizeof(PacketHeader)) != sizeof(PacketHeader)){
            return;
        }

        // get the packet
        uint8_t* buffer = read_in_packet(header);
        if (buffer == nullptr) return;
        
        // check the crc
        if (!validate_crc(buffer, header)){
            handle_bad_packet(header);
            arena_free();

            BEC_E::send_log("CRC mismatch!");

            return;
        }

        // handle the command
        if (!handle_command(header, buffer)){
            BEC_E::send_log("Unknown command");
        }

        arena_free();
    }

    void register_command(struct Command command){
        // set up a pointer to the next available spot
        static int command_pointer = 0;

        // add the command if there is room
        if (command_pointer < MAX_REGISTERED_COMMAND_NUM){
            registered_commands[command_pointer] = command;
            command_pointer ++;
        }
        else {
            while (true){
                Serial.println("Registered command buffer too small");
                delay(10 * 1000);
            }
        }
    }

    void register_loop_function(void (*loop_function)()){
        // set up a pointer to the next available spot
        static int function_pointer = 0;

        // add the function if there is room
        if (function_pointer < MAX_LOOP_FUNCTION_NUM){
            loop_functions[function_pointer] = loop_function;
            function_pointer ++;
        }
        else {
            while (true){
                Serial.println("Registered command buffer too small");
                delay(10 * 1000);
            }
        }
    }

    PacketHeader build_packet_header(uint16_t type, uint16_t packet_num, uint16_t total_packets, uint16_t payload_len, uint8_t argument_number){
        static uint32 packet_id = 0;
        PacketHeader return_header = {MAGIC, COMMAND_SET, type, packet_id, packet_num, total_packets, payload_len, argument_number};

        return return_header;
    }

    void send_TCP(PacketHeader header, uint8_t* data){
        // make sure the server is still connected
        if (!tcp_client.connected()){
            tcp_client.connect(server_ip, SERVER_PORT_TCP);
        }

        // get the total size of the data send
        size_t total_length = sizeof(PacketHeader) + header.payload_len;
        
        // make a buffer for the data
        uint8_t* buffer = new uint8_t[total_length];

        // add data to the buffer
        memcpy(buffer, &header, sizeof(PacketHeader));
        memcpy(buffer + sizeof(PacketHeader), data, header.payload_len);

        // calculate the crc
        uint16_t crc = calculate_crc16(buffer, total_length);

        // send the packet
        tcp_client.write(buffer, total_length);

        // send the crc
        tcp_client.write((const uint8_t*)&crc, sizeof(crc));

        delete[] buffer;
    }

    void send_UDP(PacketHeader header, void* data){
        // get the total size of the data send
        size_t total_length = sizeof(PacketHeader) + header.payload_len;
        
        // make a buffer for the data
        uint8_t* buffer = new uint8_t[total_length];

        // add data to the buffer
        memcpy(buffer, &header, sizeof(PacketHeader));
        memcpy(buffer + sizeof(PacketHeader), data, header.payload_len);

        // calculate the crc
        uint16_t crc = calculate_crc16(buffer, total_length);

        // start packet to the server
        udp_client.beginPacket(server_ip, SERVER_PORT_UDP);
        
        // add the packet
        udp_client.write(buffer, total_length);

        // add the crc
        udp_client.write((const uint8_t*)&crc, sizeof(crc));
        
        // send the packet
        udp_client.endPacket();
    }

    void send_log(const char* message){
        // build the header
        PacketHeader header = build_packet_header(LOG_MESSAGE, 0, 1, strlen(message), 1);
        
        // send the packet
        send_TCP(header, (uint8_t*)message);
    }

    void safe_delay(unsigned long milli_delay){
        unsigned long start_millis = millis();

        while (millis() - start_millis > milli_delay){
            main_loop();
        }
    }
} // BEC_E namespace

void run_loop_functions(){
    // loop until a null function pointer
    for (int i = 0; i < MAX_LOOP_FUNCTION_NUM; i++){
        if (loop_functions[i] == nullptr) return;

        loop_functions[i]();
    }
}

uint8_t* read_in_packet(PacketHeader header){
    // allocate memory for the full packet
    uint16_t total_len = sizeof(PacketHeader) + header.payload_len;
    uint8_t* buffer = arena_malloc(total_len);
    if (buffer == nullptr) return nullptr;

    // copy in the header
    memcpy(buffer, &header, sizeof(PacketHeader));

    // read in the rest of the packet
    int received = tcp_client.readBytes((char*)buffer + sizeof(PacketHeader), header.payload_len);

    // verify the length
    if (received != header.payload_len) {
        return nullptr;
    }

    return buffer;
}