#pragma once

#include <Arduino.h>

#ifdef BEC_E_DEBUG

    #define DBG_INIT(baud)        Serial.begin(baud)
    #define DBG_PRINT(x)          Serial.print(x)
    #define DBG_PRINTLN(x)        Serial.println(x)
    #define DBG_PRINTF(...)       Serial.printf(__VA_ARGS__)

    #define DBG_TRACE() Serial.printf("[TRACE] %s:%d\n", __FILE__, __LINE__)

    #define DBG_ASSERT(expr) \
        do { \
            if (!(expr)) { \
                Serial.printf("[ASSERT] %s:%d : %s\n", \
                    __FILE__, __LINE__, #expr); \
                while (true) delay(1000); \
            } \
        } while (0)

#else
    // compiled completely out
    #define DBG_INIT(baud)        ((void)0)
    #define DBG_PRINT(x)          ((void)0)
    #define DBG_PRINTLN(x)        ((void)0)
    #define DBG_PRINTF(...)       ((void)0)
    #define DBG_TRACE()           ((void)0)
    #define DBG_ASSERT(expr)      ((void)0)
#endif
