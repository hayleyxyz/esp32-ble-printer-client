#pragma once

#include <cstdint>

enum class BLEPrinterStatus : uint8_t
{
    Ready = 0,
    Busy = 0b10000000,
    NoPaper = 0b00000001,
    CoverOpen = 0b00000010,
    Overheat = 0b00000100,
    LowPower = 0b00001000,
    Unknown
};