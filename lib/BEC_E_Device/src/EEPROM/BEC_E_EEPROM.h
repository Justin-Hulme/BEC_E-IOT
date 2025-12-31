#pragma once

#include "BEC_E_Device.h"

#define EEPROM_MAGIC 0x42
#define EEPROM_VERSION 0

// function prototypes for internal functions
void save_credentials(const char*, const char*, const char*);
void clear_EEPROM();