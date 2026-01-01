#include <ESP8266WiFi.h>
#include <ESP8266httpUpdate.h>

#include "Commands.h"

#include "BEC_E_Device.h"
#include "Network/Network.h"
#include "EEPROM/BEC_E_EEPROM.h"
#include "Areana/Arena.h"

// array of built in commands
Command built_in_commands[] = {
    {"Restart",       65534, STRONG_BUTTON, nullptr, 0, handle_restart},
    {"Update",        65533, STRONG_BUTTON, nullptr, 0, handle_update},
    {"Send Commands", 65532, HIDDEN,        nullptr, 0, handle_send_commands},
    {"Send Name",     65531, HIDDEN,        nullptr, 0, handle_send_name},
    {"Factory Reset", 65530, STRONG_BUTTON, nullptr, 0, handle_factory_reset},
};

// array of registered commands defaulting to a null command
Command registered_commands [MAX_REGISTERED_COMMAND_NUM];

void handle_restart(ArgValue _args[], uint8_t _arg_number){
    ESP.restart();
}

// TODO spit into multiple functions
void handle_update(ArgValue _args[], uint8 _arg_number){
    WiFiClient client;
    HTTPClient http;

    BEC_E::send_log("Checking for updates");

    // set up path for ota version file
    uint16_t ota_version_path_len = SERVER_IP_SIZE + strlen("/IOT/firmware/") + strlen(DEVICE_NAME) + strlen("/version.txt");
    char* ota_version_path = new char[ota_version_path_len];
    snprintf(ota_version_path, ota_version_path_len, "%s/IOT/firmware/%s_version.txt", server_ip, DEVICE_NAME);
    
    // set up path for ota firmware file
    uint16_t ota_firmware_path_len = SERVER_IP_SIZE + strlen("/IOT/firmware/") + strlen(DEVICE_NAME) + strlen("/firmware.txt");
    char* ota_firmware_path = new char[ota_firmware_path_len];
    snprintf(ota_firmware_path, ota_version_path_len, "%s/IOT/firmware/%s/firmware.txt", server_ip, DEVICE_NAME);
    
    // check for update
    if (http.begin(client, ota_version_path)){
        int httpCode = http.GET();

        // make sure it was successful
        if (httpCode == 200) {
            // get the version from the file
            String new_version = http.getString();
            new_version.trim();

            // match it to the saved version
            if (new_version != CURRENT_VERSION){
                BEC_E::send_log("New version available! Starting OTA");

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

void handle_send_name(ArgValue _args[], uint8 _arg_number){
    const char* name = DEVICE_NAME "_" DEVICE_ID;

    // build the header
    PacketHeader header = BEC_E::build_packet_header(SEND_NAME, 0, 1, strlen(name), 1);
    
    // send the packet
    BEC_E::send_TCP(header, (uint8_t*)name);
}

void handle_factory_reset(ArgValue _args[], uint8 _arg_number){
    clear_EEPROM();

    ESP.restart();
}

bool handle_command(PacketHeader header, uint8_t* buffer){
    // get the number of commands
    size_t built_in_commands_len = sizeof(built_in_commands)/sizeof(built_in_commands[0]);

    // find what command it is trying to run
    for (int i = 0; i < built_in_commands_len; i++){
        if (check_command(built_in_commands[i], header, buffer)) return true;
    }

    for (int i = 0; i < MAX_REGISTERED_COMMAND_NUM; i++){
        if (check_command(registered_commands[i], header, buffer)) return true;
    }

    return false;
}

bool check_command(const Command& command, const PacketHeader& header, uint8_t* buffer){
    if (command.type != header.type){
        return false;
    }

    // get the position of the payload
    uint8_t* payload = buffer + sizeof(PacketHeader);

    // create array for arguments
    ArgValue* args = (ArgValue*)arena_malloc(header.argument_number * sizeof(ArgValue));
    
    // make sure that memory allocation worked
    if (args == nullptr) {
        BEC_E::send_log("Arena out of memory for string pointers");
        return false;
    }

    // add all the arguments
    for (int j = 0; j < header.argument_number; j++) {
        payload += parse_argument(args[j], payload);
    }

    // run the function
    command.receive_command_function(args, header.argument_number);

    return true;
}

uint16_t parse_argument(ArgValue& arg, uint8_t* payload){
    switch (*payload){
        case Argument::BOOL:
            arg.bool_val = *(bool*)(payload + 1);
            return 1 + sizeof(bool);
        case Argument::INT8:
            arg.int8_val = *(int8_t*)(payload + 1);
            return 1 + sizeof(int8_t);
        case Argument::INT16:
            arg.int16_val = *(int16_t*)(payload + 1);
            return 1 + sizeof(int16_t);
        case Argument::INT32:
            arg.int32_val = *(int32_t*)(payload + 1);
            return 1 + sizeof(int32_t);
        case Argument::UINT8:
            arg.uint8_val = *(uint8_t*)(payload + 1);
            return 1 + sizeof(uint8_t);
        case Argument::UINT16:
            arg.uint16_val = *(uint16_t*)(payload + 1);
            return 1 + sizeof(uint16_t);
        case Argument::UINT32:
            arg.uint32_val = *(uint32_t*)(payload + 1);
            return 1 + sizeof(uint32_t);
        case Argument::FLOAT:
            arg.float_val = *(float*)(payload + 1);
            return 1 + sizeof(float);
        case Argument::COLOR:
            arg.color_val = *(Color*)(payload + 1);
            return 1 + sizeof(Color);
        case Argument::STRING: {
            uint16_t str_len = *(uint16_t*)(payload + 1);
            
            // Allocate from arena with room for null terminator
            char* str = (char*)arena_malloc(str_len + 1);
            if (!str) {
                BEC_E::send_log("Arena out of memory for string");
                arg.str_val = nullptr;
                return 1 + 2 + str_len; // still advance payload pointer to avoid stuck parsing
            }
            
            // copy the string over
            memcpy(str, payload + 3, str_len);
            str[str_len] = '\0';
            
            // add the string to the argument
            arg.str_val = str;
            
            return 1 + 2 + str_len;
        }
        default:
            BEC_E::send_log("argument type not defined");
            return 1;
    }
}

void init_registered_commands(){
    Command default_command = {nullptr, 65535, HIDDEN, nullptr, 0, nullptr};
    
    for (int i = 0; i < MAX_REGISTERED_COMMAND_NUM; i++){
        registered_commands[i] = default_command;
    }
}