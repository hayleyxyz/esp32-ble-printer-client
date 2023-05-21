#pragma once

#include <cstdlib>

class Checksum8Bit
{
public:
    static uint8_t calculate(uint8_t* data, size_t length);

//private:
    static const uint8_t crc_table[256];
};