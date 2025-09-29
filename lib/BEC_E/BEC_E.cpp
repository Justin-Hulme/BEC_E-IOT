#include "BEC_E.h"

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>

#include <EEPROM.h>

// function prototypes for build in commands
void handle_restart(ArgValue *, uint8);
void handle_update(ArgValue *, uint8);
void handle_send_commands(ArgValue *, uint8);
void handle_send_name(ArgValue *, uint8);
void handle_factory_reset(ArgValue *, uint8);

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

// give everything access to the server ip, ssid, and password
char ssid[SSID_SIZE];
char password[WIFI_PASSWORD_SIZE];
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
void clear_EEPROM();

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
            if (attempts >= TCP_CONNECTION_ATTMEPTS){
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
            send_TCP(header, &buffer);
        }
    }

    void main_loop(){
        if (tcp_client.connected()){
            if (tcp_client.available()){
                // read in header
                PacketHeader header;
                if (tcp_client.readBytes((char*)&header, sizeof(PacketHeader)) == sizeof(PacketHeader)){
                    // allocate memory for the full packet
                    uint16_t total_len = sizeof(PacketHeader) + header.payload_len;
                    uint8_t* buffer = new uint8_t[total_len];

                    // copy in the header
                    memcpy(buffer, &header, sizeof(PacketHeader));

                    // read in the rest of the packet
                    int received = tcp_client.readBytes((char*)buffer + sizeof(PacketHeader), header.payload_len);
                    bool bad_packet = false;
                    
                    // if we recieved the correct amount of data
                    if (received == header.payload_len) {
                        // Validate CRC
                        uint16_t crc_received = *(uint16_t*)(buffer + total_len - 2);
                        uint16_t crc_computed = calculate_crc16(buffer, total_len - 2);

                        //if the CRC was correct
                        if (crc_received == crc_computed) {
                            // get the number of commands
                            size_t built_in_commands_len = sizeof(built_in_commands)/sizeof(built_in_commands[0]);
                            
                            bool found = false;
                            
                            // find what command it is trying to run
                            // TODO: figure out how to also run non built in commands
                            for (int i = 0; i < built_in_commands_len; i++){
                                // if the current command matches the recieved command
                                if (built_in_commands[i].type == header.type){
                                    found = true;

                                    // get the position of the payload
                                    uint8_t* payload = buffer + sizeof(PacketHeader);

                                    // create array for arguments
                                    ArgValue* args = new ArgValue[header.argument_number];
                                    char** arg_string_pointers = new char*[header.argument_number];

                                    for (int j = 0; j < header.argument_number; j++){
                                        uint8_t arg_type = *(payload + 1);
                                        arg_string_pointers[j] = NULL;

                                        switch (arg_type){
                                        case Argument::BOOL:
                                            memcpy(&args[j].bool_val, payload + 2, sizeof(ArgValue));
                                            break;
                                        case Argument::INT8:
                                            memcpy(&args[j].int8_val, payload + 2, sizeof(ArgValue));
                                            break;
                                        case Argument::INT16:
                                            memcpy(&args[j].int16_val, payload + 2, sizeof(ArgValue));
                                            break;
                                        case Argument::INT32:
                                            memcpy(&args[j].int32_val, payload + 2, sizeof(ArgValue));
                                            break;
                                        case Argument::UINT8:
                                            memcpy(&args[j].uint8_val, payload + 2, sizeof(ArgValue));
                                            break;
                                        case Argument::UINT16:
                                            memcpy(&args[j].uint16_val, payload + 2, sizeof(ArgValue));
                                            break;
                                        case Argument::UINT32:
                                            memcpy(&args[j].uint32_val, payload + 2, sizeof(ArgValue));
                                            break;
                                        case Argument::FLOAT:
                                            memcpy(&args[j].float_val, payload + 2, sizeof(ArgValue));
                                            break;
                                        case Argument::COLOR:
                                            memcpy(&args[j].color_val, payload + 2, sizeof(ArgValue));
                                            break;
                                        case Argument::STRING:
                                            // get the length
                                            uint16 string_size;
                                            memcpy(&string_size, payload + 2, sizeof(uint16_t));

                                            // build the string
                                            char * string = new char[string_size + 1]; // plus one because it is coming in as length based and we want it to be null terminated
                                            memcpy(string, (char*)payload + 2 + sizeof(uint16_t), string_size);

                                            // make sure it null terminates
                                            string[string_size] = '\0';

                                            args[j].str_val = string;
                                            arg_string_pointers[j] = string;
                                            break;
                                        default:
                                            send_log("argument type not defined");
                                            break;
                                        }
                                    }
                                    built_in_commands[i].recieve_command_function(args, header.argument_number);

                                    // free strings
                                    for (int j = 0; j < header.argument_number; j++){
                                        delete[] arg_string_pointers[j];
                                    }
                                    delete[] arg_string_pointers;
                                    delete[] args;
                                }
                            }

                            if (!found){
                                BEC_E::send_log("Unknown command");
                                return;
                            }

                            //handle_packet(&header, buffer + sizeof(PacketHeader), header.payload_len - 2);
                        } else {
                            BEC_E::send_log("CRC mismatch!");
                            bad_packet = true;
                        }
                    } else {
                        BEC_E::send_log("Incomplete payload received.");
                        bad_packet = true;
                    }
                    
                    if (bad_packet){
                        // build the buffer and specify the type
                        uint8_t buffer[1 + sizeof(uint32_t)];
                        buffer[0] = Argument::UINT32;

                        // copy the packed id into the buffer
                        memcpy(buffer + 1, &header.packet_id, sizeof(uint32_t));

                        // build the header
                        PacketHeader resend_header = build_packet_header(RESEND, 0, 1, sizeof(buffer), 1);

                        BEC_E::send_TCP(resend_header, &buffer);
                    }

                    delete[] buffer;
                }
            }
        }

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

    PacketHeader build_packet_header(uint16_t type, uint16_t packet_num, uint16_t total_packets, uint16_t payload_len, uint8_t argument_number){
        static uint32 packet_id = 0;
        PacketHeader return_header = {MAGIC, COMMAND_SET, type, packet_id, packet_num, total_packets, payload_len, argument_number};
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

    void send_log(const char* message){
        // build the header
        PacketHeader header = build_packet_header(LOG_MESSAGE, 0, 1, strlen(message), 1);
        
        // send the packet
        send_TCP(header, message);
    }

    void safe_delay(unsigned long milli_delay){
        unsigned long start_millis = millis();

        while (millis() - start_millis > milli_delay){
            main_loop();
        }
    }
} // BEC_E namespace

void handle_restart(ArgValue _args[], uint8 _arg_number){
    ESP.restart();
}

void handle_update(ArgValue _args[], uint8 _arg_number){
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

void handle_send_commands(ArgValue _args[], uint8 _arg_number) {
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
    uint16_t name_len = strlen(cmd.name);

    //                    arg type          string len         string     command id       command type
    uint16_t total_size = sizeof(uint8_t) + sizeof(uint16_t) + name_len + sizeof(cmd.id) + sizeof(cmd.type);

    // handle additional arguments
    switch (cmd.type){
        case SLIDER_UINT8:
            // make sure it has the right numnber of arguments
            if (cmd.additional_arg_num != 2){
                BEC_E::send_log("Wrong number of args for SLIDER_UINT8");
                return;
            }
            
            // add the type and size of the 2 arguments
            total_size += 2 * (sizeof(uint8_t) + sizeof(ArgValue().uint8_val));
        break;
        case DROPDOWN:
            // make sure it has the right numnber of arguments
            if (cmd.additional_arg_num < 1){
                BEC_E::send_log("not enough args for dropdown");
                return;
            }

            // add enough space for all of the arguments
            for (int i = 0; i < cmd.additional_arg_num; i++){
                total_size += sizeof(uint8_t); //for the argument type
                total_size += sizeof(uint16_t); //for the string len
                total_size += strlen(cmd.additional_args[i].str_val);
            }
        break;
    }

    // reusable way of adding the argument type
    Argument::arg_type argument_type = Argument::STRING;

    // set up the buffer
    uint8_t* buffer = new uint8_t[total_size];
    uint16_t offset = 0;
    
    // add the argument type
    memcpy(buffer + offset, &argument_type, sizeof(uint8_t));
    offset += sizeof(argument_type);

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
        case SLIDER_UINT8:
            argument_type = Argument::UINT8;

            // add first argument as type uint8
            memcpy(buffer + offset, &argument_type, sizeof(argument_type));
            offset += sizeof(argument_type);

            // add the starting value
            memcpy(buffer + offset, &cmd.additional_args[0].uint8_val, sizeof(ArgValue().uint8_val));
            offset += sizeof(ArgValue().uint8_val);
            
            // add second argument as type uint8
            memcpy(buffer + offset, &argument_type, sizeof(argument_type));
            offset += sizeof(argument_type);

            // add the ending value
            memcpy(buffer + offset, &cmd.additional_args[1].uint8_val, sizeof(ArgValue().uint8_val));
            offset += sizeof(ArgValue().uint8_val);
        break;
        case DROPDOWN:
            argument_type = Argument::STRING;
            
            for (int i = 0; i < cmd.additional_arg_num; i++){
                // get the legnth of the string
                uint8_t str_len = strlen(cmd.additional_args[i].str_val);
                
                // add argument as type string
                memcpy(buffer + offset, &argument_type, sizeof(argument_type));
                offset += sizeof(argument_type);

                // add string length to the buffer
                memcpy(buffer + offset, &str_len, sizeof(uint8_t));
                offset += sizeof(uint8_t);

                // add the string to the buffer
                memcpy(buffer + offset, cmd.additional_args[i].str_val, sizeof(ArgValue().str_val));
                offset += sizeof(ArgValue().str_val);
            }
        break;
    }

    PacketHeader header = BEC_E::build_packet_header(SEND_COMMAND, 0, 1, total_size, 1 + cmd.additional_arg_num);
    BEC_E::send_TCP(header, buffer);

    delete[] buffer;
}

void handle_send_name(ArgValue _args[], uint8 _arg_number){
    char* name = DEVICE_NAME "_" DEVICE_ID;

    // build the header
    PacketHeader header = BEC_E::build_packet_header(SEND_NAME, 0, 1, strlen(name), 1);
    
    // send the packet
    BEC_E::send_TCP(header, name);
}

void handle_factory_reset(ArgValue _args[], uint8 _arg_number){
    clear_EEPROM();

    ESP.restart();
}

void clear_EEPROM(){
    for (int i = 0; i < EEPROM.length(); i++) {
        EEPROM.write(i, 0xFF);
    }

    EEPROM.commit();
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
    // Assume you have these defined somewhere
    String html = "<form action=\"/submit\" method=\"POST\">";
    html += "SSID: <input name=\"ssid\" maxlength=" + String(SSID_SIZE - 1) + " value=\"";
    html += ssid;
    html += "\"><br>";

    html += "Password: <input type=\"password\" name=\"pass\" maxlength=" + String(WIFI_PASSWORD_SIZE - 1) + " value=\"";
    html += password;
    html += "\"><br>";

    html += "Server IP: <input name=\"server_ip\" maxlength=" + String(SERVER_IP_SIZE - 1) + " value=\"";
    html += server_ip;
    html += "\"><br>";

    html += "<input type=\"submit\"></form>";

    server.send(200, "text/html", html);
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
        EEPROM.write(2 + i, ssid[i]);
    }

    // Write password
    for (int i = 0; i < WIFI_PASSWORD_SIZE; i++){
        EEPROM.write(2 + SSID_SIZE + i, password[i]);
    }

    // Write server IP
    for (int i = 0; i < SERVER_IP_SIZE; i++){
        EEPROM.write(2 + SSID_SIZE + WIFI_PASSWORD_SIZE + i, server_address[i]);
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