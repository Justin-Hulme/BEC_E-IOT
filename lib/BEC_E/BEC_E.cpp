#include "BEC_E.h"

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>

#include <EEPROM.h>

// function prototypes for build in commands
void handle_restart(ArgValue *);
void handle_update(ArgValue *);
void handle_send_commands(ArgValue *);
void handle_send_name(ArgValue *);
void handle_factory_reset(ArgValue *);

// array of built in commands
Command built_in_commands[] = {
    {"Restart",       65534, STRONG_BUTTON, nullptr, 0, handle_restart},
    {"Update",        65533, STRONG_BUTTON, nullptr, 0, handle_update},
    {"Send Commands", 65532, HIDDEN,        nullptr, 0, handle_send_commands},
    {"Send Name",     65531, HIDDEN,        nullptr, 0, handle_send_name},
    {"Factory Reset", 65530, STRONG_BUTTON, nullptr, 0, handle_factory_reset},
};

// array of registered commands defaulting to a null command
Command registered_commands [MAX_REGISTERED_COMMAND_NUM] = {nullptr, 65535, HIDDEN, nullptr, 0, nullptr};

// array of loop functions defaulting to a null function
void (*loop_functions[MAX_LOOP_FUNCTION_NUM])() = {nullptr};

// give everything access to the server ip
char server_ip[SERVER_IP_SIZE];

// communication conrollers
WiFiClient tcp_client;
WiFiUDP udp_client;

// function prototypes for internal functions
void send_single_command(const Command&);
bool connect_wifi(char*, char*);
void run_AP();
void save_credentials(const char*, const char*, const char*);
uint16_t calculate_crc16(const uint8_t* data, size_t length);

namespace BEC_E {
    void main_setup(){
        Serial.begin(115200);
        EEPROM.begin(1 + SSID_SIZE + WIFI_PASSWORD_SIZE + SERVER_IP_SIZE);

        // check for magic to see if the eeprom has been reset
        if (EEPROM.read(0) != EEPROM_MAGIC){
            run_AP();
        }

        // set up buffers for ssid, and password
        char ssid[SSID_SIZE];
        char password[WIFI_PASSWORD_SIZE];

        // read in the ssid
        for (int i = 0; i < SSID_SIZE; i++){
            ssid[i] = EEPROM.read(1 + i);
        }

        // read in the password
        for (int i = 0; i < WIFI_PASSWORD_SIZE; i++){
            password[i] = EEPROM.read(1 + SSID_SIZE + i);
        }

        // read in the server ip
        for (int i = 0; i < SERVER_IP_SIZE; i++){
            server_ip[i] = EEPROM.read(1 + SSID_SIZE + WIFI_PASSWORD_SIZE + i);
        }

        // try to connect to wifi and set up AP if the saved values fail
        if (! connect_wifi(ssid, password)){
            run_AP();
        }

        // connect to tcp
        if (tcp_client.connect(server_ip, SERVER_PORT_TCP)){
            const char* message = DEVICE_NAME "_" DEVICE_ID " CONNECTED";
            BEC_E::send_log(message);
        }
        else {
            // TODO: figure out how to fail gracefully
        }

        // connect to udp
        if (USE_UDP){
            udp_client.begin(SERVER_PORT_UDP);

            uint16_t port = SERVER_PORT_UDP;

            // build the header
            PacketHeader header = build_packet_header(ESTABLISH_UDP, 0, 1, sizeof(port));
            
            // tell the server to start listening to UDP
            send_TCP(header, &port);
        }
    }

    void main_loop(){
        // TODO: implement main_loop
        // TODO: implement heartbeat packet

        // run all of the added loop functions
        for (int i = 0; i < MAX_LOOP_FUNCTION_NUM; i++){
            if (loop_functions[i]) {
                loop_functions[i]();
            }
        }
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

    PacketHeader build_packet_header(uint16_t type, uint16_t packet_num, uint16_t total_packets, uint16_t payload_len){
        static uint32 packet_id = 0;
        PacketHeader return_header = {MAGIC, COMMAND_SET, type, packet_id, packet_num, total_packets, payload_len};
    }

    void send_log(const char* message){
        // build the header
        PacketHeader header = build_packet_header(LOG_MESSAGE, 0, 1, strlen(message));
        
        // send the packet
        send_TCP(header, message);
    }

    void send_TCP(PacketHeader header, const void* data){
        // make sure the server is still connected
        if (!tcp_client.connected()){
            tcp_client.connect(server_ip, SERVER_PORT_TCP);
        }

        // get the total size of the data send
        size_t total_legth = sizeof(PacketHeader) + header.payload_len;
        
        // make a buffer for the data
        uint8_t* buffer = new uint8_t[total_legth];

        // add data to the buffer
        memcpy(buffer, &header, sizeof(PacketHeader));
        memcpy(buffer + sizeof(PacketHeader), data, header.payload_len);

        // calculate the crc
        uint16_t crc = calculate_crc16(buffer, total_legth);

        // send the packet
        tcp_client.write(buffer, total_legth);

        // send the crc
        tcp_client.write((const uint8_t*)&crc, sizeof(crc));

        delete[] buffer;
    }

    void send_UDP(PacketHeader header, void* data){
        // get the total size of the data send
        size_t total_legth = sizeof(PacketHeader) + header.payload_len;
        
        // make a buffer for the data
        uint8_t* buffer = new uint8_t[total_legth];

        // add data to the buffer
        memcpy(buffer, &header, sizeof(PacketHeader));
        memcpy(buffer + sizeof(PacketHeader), data, header.payload_len);

        // calculate the crc
        uint16_t crc = calculate_crc16(buffer, total_legth);

        // start packet to the server
        udp_client.beginPacket(server_ip, SERVER_PORT_UDP);
        
        // add the packet
        udp_client.write(buffer, total_legth);

        // add the crc
        udp_client.write((const uint8_t*)&crc, sizeof(crc));
        
        // send the packet
        udp_client.endPacket();
    }

    void safe_delay(unsigned long milli_delay){
        unsigned long start_millis = millis();

        while (millis() - start_millis > milli_delay){
            main_loop();
        }
    }
}

void handle_restart(ArgValue _args[]){
    ESP.restart();
}

void handle_update(ArgValue _args[]){
    WiFiClient client;
    HTTPClient http;

    BEC_E::send_log("Checking for updates");

    // set up path for ota version file
    uint16_t ota_version_path_len = SERVER_IP_SIZE + strlen("/IOT/firmware/") + strlen(DEVICE_NAME) + strlen("/version.txt");
    char* ota_version_path = new char[ota_version_path_len];
    snprintf(ota_version_path, sizeof(ota_version_path), "%s/IOT/firmware/%s_version.txt", server_ip, DEVICE_NAME);
    
    // set up path for ota firmware file
    uint16_t ota_firmware_path_len = SERVER_IP_SIZE + strlen("/IOT/firmware/") + strlen(DEVICE_NAME) + strlen("/firmware.txt");
    char* ota_firmware_path = new char[ota_firmware_path_len];
    snprintf(ota_firmware_path, sizeof(ota_firmware_path), "%s/IOT/firmware/%s/firmware.txt", server_ip, DEVICE_NAME);
    
    // check for update
    if (http.begin(client, ota_version_path)){
        int httpCode = http.GET();

        // make sure it was sucessful
        if (httpCode == 200) {
            // get the version from the file
            String new_version = http.getString();
            new_version.trim();

            // match it to the saved version
            if (new_version != CURRENT_VERSION){
                BEC_E::send_log("New version avialable! Starting OTA");

                // start the update
                t_httpUpdate_return result = ESPhttpUpdate.update(client, ota_firmware_path);

                // handle the result of the update
                switch (result){
                    case HTTP_UPDATE_FAILED:
                        BEC_E::send_log("Update failed");
                        BEC_E::send_log(ESPhttpUpdate.getLastErrorString().c_str());
                    break;
                    case HTTP_UPDATE_NO_UPDATES:
                        BEC_E::send_log("No updates available");
                    break;
                    case HTTP_UPDATE_OK:
                        BEC_E::send_log("Update successful. Rebooting");
                    break;
                }
            } 
            else {
                BEC_E::send_log("Firmware is up-to-date");
            }
        }
        else {
            BEC_E::send_log("Failed to check update version");
        }

        // clean up
        http.end();
    }

}

void handle_send_commands(ArgValue _args[]) {
    for (int i = 0; i < MAX_REGISTERED_COMMAND_NUM; i++) {
        if (registered_commands[i].id == 65535) break;
        send_single_command(registered_commands[i]);
    }

    size_t built_in_count = sizeof(built_in_commands) / sizeof(built_in_commands[0]);
    for (size_t i = 0; i < built_in_count; i++) {
        send_single_command(built_in_commands[i]);
    }
}

void send_single_command(const Command& cmd) {
    // get the normal size
    uint8_t name_len = strlen(cmd.name);
    uint16_t total_size = sizeof(uint8_t) + name_len + sizeof(cmd.id) + sizeof(cmd.type);

    // handle additional arguments
    switch (cmd.type){
        case SLIDER:
            // make sure it has the right numnber of arguments
            if (cmd.additional_arg_num != 2){
                BEC_E::send_log("Wrong number of args for slider");
                return;
            }
            
            // add the size of the 2 arguments
            total_size += 2 * sizeof(ArgValue().uint_val);
        break;
        case DROPDOWN:
            // make sure it has the right numnber of arguments
            if (cmd.additional_arg_num < 1){
                BEC_E::send_log("not enough args for dropdown");
                return;
            }

            // add enough space for all of the arguments
            for (int i = 0; i < cmd.additional_arg_num; i++){
                total_size += sizeof(uint8); //for the string len
                total_size += strlen(cmd.additional_args[i].str_val);
            }
        break;
    }

    // set up the buffer
    uint8_t* buffer = new uint8_t[total_size];
    uint16_t offset = 0;
    
    // add the length of the name
    memcpy(buffer + offset, &name_len, sizeof(name_len));
    offset += sizeof(name_len);

    // add the name
    memcpy(buffer + offset, cmd.name, name_len);
    offset += name_len;

    // add the id
    memcpy(buffer + offset, &cmd.id, sizeof(cmd.id));
    offset += sizeof(cmd.id);

    // add the command type
    memcpy(buffer + offset, &cmd.type, sizeof(cmd.type));
    offset += sizeof(cmd.type);

    // handle additional arguments
    switch (cmd.type){
        case SLIDER:
            // add the starting value
            memcpy(buffer + offset, &cmd.additional_args[0].int_val, sizeof(ArgValue().int_val));
            offset += sizeof(ArgValue().int_val);
            
            // add the ending value
            memcpy(buffer + offset, &cmd.additional_args[1].int_val, sizeof(ArgValue().int_val));
            offset += sizeof(ArgValue().int_val);
        break;
        case DROPDOWN:
            for (int i = 0; i < cmd.additional_arg_num; i++){
                // get the legnth of the string
                uint8_t str_len = strlen(cmd.additional_args[i].str_val);

                // add string length to the buffer
                memcpy(buffer + offset, &str_len, sizeof(uint8_t));
                offset += sizeof(uint8_t);

                // add the string to the buffer
                memcpy(buffer + offset, cmd.additional_args[i].str_val, sizeof(ArgValue().str_val));
                offset += sizeof(ArgValue().str_val);
            }
        break;
    }

    PacketHeader header = BEC_E::build_packet_header(SEND_COMMAND, 0, 1, total_size);
    BEC_E::send_TCP(header, buffer);

    delete[] buffer;
}

void handle_send_name(ArgValue *){
    // TODO: implement handle_send_name
}

void handle_factory_reset(ArgValue *){
    // TODO: implement handle_factory_reset
}

bool connect_wifi(char* ssid, char* password){
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

    WiFi.begin(ssid, password);
    
    int attempts = 0;
    
    // loop untill connected or until timeout
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");

        if (attempts >= 2 * WIFI_TIMEOUT_SECONDS){
            return false;
        }
        else{
            attempts ++;
        }
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

    return true;
}

ESP8266WebServer server(80);

void run_AP(){
    const IPAddress local_ip(192, 168, 0, 1);     // ESP's own IP
    const IPAddress gateway(192, 168, 0, 1);      // Gateway (same as IP for SoftAP)
    const IPAddress subnet(255, 255, 255, 0);     // Subnet mask
    
    WiFi.softAPConfig(local_ip, gateway, subnet);

    WiFi.softAP(DEVICE_NAME "_" DEVICE_ID);
    
    server.on("/", handleRoot);
    server.on("/submit", handleSubmit);
    server.begin();
}

void handleRoot() {
    server.send(200, "text/html",
        "<form action=\"/submit\" method=\"POST\">"
        "SSID: <input name=\"ssid\" length=32><br>"
        "Password: <input name=\"pass\" length=64><br>"
        "Server IP: <input name=\"server_ip\" length=16><br>"
        "<input type=\"submit\">"
        "</form>"
    );
}

void handleSubmit() {
    char ssid[SSID_SIZE];
    char pass[WIFI_PASSWORD_SIZE];
    char ip[SERVER_IP_SIZE];

    // copy the ssid
    strncpy(ssid, server.arg("ssid").c_str(), SSID_SIZE);
    ssid[SSID_SIZE - 1] = '\0';

    // copy password
    strncpy(pass, server.arg("pass").c_str(), WIFI_PASSWORD_SIZE);
    pass[WIFI_PASSWORD_SIZE - 1] = '\0';

    // copy servr ip
    strncpy(ip, server.arg("server_ip").c_str(), SERVER_IP_SIZE);
    ip[SERVER_IP_SIZE - 1] = '\0';

    save_credentials(ssid, pass, ip);

    server.send(200, "text/html", "Saved. Restarting...");
    delay(1000);
    ESP.restart();
}

void save_credentials(const char* ssid, const char* password, const char* server_address) {
    if (EEPROM.read(0) != EEPROM_MAGIC){
        EEPROM.write(0, EEPROM_MAGIC);
    }

    // Write SSID
    for (int i = 0; i < SSID_SIZE; i++){
        EEPROM.write(1 + i, ssid[i]);
    }

    // Write password
    for (int i = 0; i < WIFI_PASSWORD_SIZE; i++){
        EEPROM.write(1 + SSID_SIZE + i, password[i]);
    }

    // Write server IP
    for (int i = 0; i < SERVER_IP_SIZE; i++){
        EEPROM.write(1 + SSID_SIZE + WIFI_PASSWORD_SIZE + i, server_address[i]);
    }

    EEPROM.commit();
}

uint16_t calculate_crc16(const uint8_t* data, size_t length) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return crc;
}