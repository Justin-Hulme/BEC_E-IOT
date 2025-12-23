#include "Arena.h"

uint8_t packet_arena[PACKET_ARENA_SIZE];
uint16_t arena_pointer;

uint8_t* arena_malloc(uint16_t allocation_size) {
    // Align to 4 bytes
    arena_pointer = (arena_pointer + 3) & ~3;

    if (arena_pointer + allocation_size > PACKET_ARENA_SIZE) {
        return nullptr;
    }

    uint8_t* return_pointer = packet_arena + arena_pointer;
    arena_pointer += allocation_size;
    
    return return_pointer;
}

void arena_free(){
    arena_pointer = 0;
}