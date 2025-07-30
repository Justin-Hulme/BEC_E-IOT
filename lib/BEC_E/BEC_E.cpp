#include "BEC_E.h"

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ESP8266WebServer.h>

#include <EEPROM.h>

// function prototypes for build in commands
void handle_restart(ArgValue *);
void handle_update(ArgValue *);
void handle_register(ArgValue *);
void send_commands(ArgValue *);

// array of built in commands
Command built_in_commands[] = {
    {"Restart",       65534, VOID_C, handle_restart},
    {"Update",        65533, VOID_C, handle_update},
    {"Register",      65532, VOID_C, handle_register},
    {"Send Commands", 65531, VOID_C, send_commands}
};

// array of registered commands
Command registered_commands [MAX_REGISTERED_COMMAND_NUM] = {nullptr, 65535, VOID_C, nullptr};

// array of loop functions
void (*loop_functions[MAX_LOOP_FUNCTION_NUM])() = {nullptr};

// give everything access to the server ip
char server_ip[SERVER_IP_SIZE];

// communication conrollers
WiFiClient tcp_client;
WiFiUDP udp_client;

// function prototypes for internal functions
bool connect_wifi(char*, char*);
void run_AP();

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
            BEC_E::send_log(message, sizeof(message));
        }
        else {
            // TODO: figure out how to fail gracefully
        }

        // connect to udp
        if (USE_UDP){
            udp_client.begin(SERVER_PORT_UDP);

            // TODO: come up with some way of telling the server the device is using udp
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

    void send_log(const char* message, size_t message_len){
        PacketHeader header = build_packet_header(LOG_MESSAGE, 0, 1, message_len);

        send_TCP(header, message, message_len);
    }

    void send_TCP(PacketHeader header, const void* data, uint16_t data_length){
        // make sure the server is still connected
        if (!tcp_client.connected()){
            tcp_client.connect(server_ip, SERVER_PORT_TCP);
        }

        // send the header and then the data
        tcp_client.write((const uint8_t*)&header, sizeof(PacketHeader));
        tcp_client.write((const uint8_t*)data, data_length);
    }

    void send_UDP(PacketHeader header, void* data, uint16_t data_length){
        // start packet to the server
        udp_client.beginPacket(server_ip, SERVER_PORT_UDP);
        
        // add the header and message
        udp_client.write((const uint8_t*)&header, sizeof(PacketHeader));
        udp_client.write((const uint8_t*)data, data_length);
        
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
    // TODO: implement handle_update
}

void handle_register(ArgValue args[]) {
    // define buffers
    char ssid[SSID_SIZE];
    char password[WIFI_PASSWORD_SIZE];
    char server_address[SERVER_IP_SIZE];

    // extract ssid from args
    strncpy(ssid, args[0].str_val, sizeof(ssid));
    ssid[sizeof(ssid)-1] = '\0';

    // extract password from args
    strncpy(password, args[1].str_val, sizeof(password));
    password[sizeof(password)-1] = '\0';

    // extract server ip from args
    strncpy(server_address, args[2].str_val, sizeof(server_address));
    server_address[sizeof(server_address)-1] = '\0';

    // mark that the eeprom has been read
    if (EEPROM.read(0) != EEPROM_MAGIC){
        EEPROM.write(0, EEPROM_MAGIC);
    }

    // write ssid to eeprom
    for (int i = 0; i < SSID_SIZE; i++){
        EEPROM.write(1 + i, ssid[i]);
    }

    // write passwrod to flash
    for (int i = 0; i < WIFI_PASSWORD_SIZE; i++){
        EEPROM.write(1 + SSID_SIZE + i, password[i]);
    }

    // write server ip to flash
    for (int i = 0; i < SERVER_IP_SIZE; i++){
        EEPROM.write(1 + SSID_SIZE + WIFI_PASSWORD_SIZE + i, server_address[i]);
    }

    EEPROM.commit();
}

void send_commands(ArgValue args[]){
    // TODO: implement sending commands
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
        "<input type=\"submit\">"
        "</form>"
    );
}

void handleSubmit() {
    // Define temporary C-style buffers
    char ssid[SSID_SIZE];
    char pass[WIFI_PASSWORD_SIZE];

    // Copy values from HTTP request
    strncpy(ssid, server.arg("ssid").c_str(), SSID_SIZE);
    ssid[SSID_SIZE - 1] = '\0';  // Ensure null termination

    strncpy(pass, server.arg("pass").c_str(), WIFI_PASSWORD_SIZE);
    pass[WIFI_PASSWORD_SIZE - 1] = '\0';  // Ensure null termination

    // Prepare ArgValue array
    ArgValue args[3];
    args[0].str_val = ssid;
    args[1].str_val = pass;
    args[2].str_val = server_ip;  // global IP already available

    // Store the data
    handle_register(args);

    // Respond and reboot
    server.send(200, "text/html", "Saved. Restarting...");
    delay(1000);
    ESP.restart();
}
