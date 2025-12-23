#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

#include "Network.h"

// give everything access to the server ip, ssid, and password
char ssid[SSID_SIZE];
char password[WIFI_PASSWORD_SIZE];
char server_ip[SERVER_IP_SIZE];

// communication controllers
WiFiClient tcp_client;
WiFiUDP udp_client;

ESP8266WebServer server(80);

bool connect_wifi(char* ssid, char* password){
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

    WiFi.begin(ssid, password);
    
    int attempts = 0;
    
    // loop until connected or until timeout
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

    // copy server ip
    strncpy(ip, server.arg("server_ip").c_str(), SERVER_IP_SIZE);
    ip[SERVER_IP_SIZE - 1] = '\0';

    save_credentials(ssid, pass, ip);

    server.send(200, "text/html", "Saved. Restarting...");
    delay(1000);
    ESP.restart();
}

// TODO split into multiple
void send_single_command(const Command& cmd) {
    // get the normal size
    uint16_t name_len = strlen(cmd.name);

    //                    arg type          string len         string     command id       command type
    uint16_t total_size = sizeof(uint8_t) + sizeof(uint16_t) + name_len + sizeof(cmd.id) + sizeof(cmd.type);

    // handle additional arguments
    switch (cmd.type){
        case SLIDER_UINT8:
            // make sure it has the right number of arguments
            if (cmd.additional_arg_num != 2){
                BEC_E::send_log("Wrong number of args for SLIDER_UINT8");
                return;
            }
            
            // add the type and size of the 2 arguments
            total_size += 2 * (sizeof(uint8_t) + sizeof(ArgValue().uint8_val));
        break;
        case DROPDOWN:
            // make sure it has the right number of arguments
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
                // get the length of the string
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

bool validate_crc(uint8_t* buffer, PacketHeader header){
    uint16_t total_len = sizeof(PacketHeader) + header.payload_len;

    // get the crc at the end of the packet
    uint16_t crc_received;
    memcpy(&crc_received, buffer + total_len - 2, sizeof(crc_received));

    // calculate the crc of the packet
    uint16_t crc_computed = calculate_crc16(buffer, total_len - 2);

    return crc_received == crc_computed;
}