#pragma once

#include "BEC_E_Device.h"

#ifndef PACKET_ARENA_SIZE
#define PACKET_ARENA_SIZE 1024
#endif

uint8_t* arena_malloc(uint16_t allocation_size);

void arena_free();