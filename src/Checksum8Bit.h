#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>

#if !defined(CHECKSUM8BIT_STATIC_TABLE)
    #define CHECKSUM8BIT_GENERATE_TABLE
#endif

#if defined(CHECKSUM8BIT_GENERATE_TABLE) || !defined(CHECKSUM8BIT_STATIC_TABLE)
    static uint8_t* crc_table = nullptr;
#elif !defined(CHECKSUM8BIT_GENERATE_TABLE) || defined(CHECKSUM8BIT_STATIC_TABLE)
    extern const uint8_t crc_table[256];
#endif

class Checksum8Bit
{
public:
    static uint8_t calculate(uint8_t* data, size_t length, uint8_t checksum = 0);

#if defined(CHECKSUM8BIT_GENERATE_TABLE) || !defined(CHECKSUM8BIT_STATIC_TABLE)
    static void generate_table(uint8_t *table) {
        uint16_t polynomial = 0x07;
        uint8_t crc = 0x80;

        memset(table, 0, 256);

        for (size_t i = 1; i < 256; i <<= 1) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ polynomial;
            } else {
                crc <<= 1;
            }

            for (size_t j = 0; j < i; j++) {
                table[i + j] = crc ^ table[j];
            }
        }
    }
#endif
};