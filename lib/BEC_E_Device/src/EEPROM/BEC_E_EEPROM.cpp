#include "BEC_E_EEPROM.h"

#include <EEPROM.h>

#include "debug.h"
#include "Network/Network.h"

void clear_EEPROM(){
    for (size_t i = 0; i < EEPROM.length(); i++) {
        EEPROM.write(i, 0xFF);
    }

    EEPROM.commit();
}

void save_credentials(const char* ssid, const char* password, const char* server_address) {
    if (EEPROM.read(0) != EEPROM_MAGIC){
        DBG_PRINTLN("Setting magic number");
        EEPROM.write(0, EEPROM_MAGIC);
    }

    // Write SSID
    DBG_PRINTF("Saving the SSID (%s)\n", ssid);
    for (int i = 0; i < SSID_SIZE; i++){
        EEPROM.write(2 + i, ssid[i]);
    }

    // Write password
    DBG_PRINTF("Saving the password (%s)\n", password);
    for (int i = 0; i < WIFI_PASSWORD_SIZE; i++){
        EEPROM.write(2 + SSID_SIZE + i, password[i]);
    }

    // Write server IP
    DBG_PRINTF("Saving the server IP (%s)\n", server_address);
    for (int i = 0; i < SERVER_IP_SIZE; i++){
        EEPROM.write(2 + SSID_SIZE + WIFI_PASSWORD_SIZE + i, server_address[i]);
    }

    EEPROM.commit();
}