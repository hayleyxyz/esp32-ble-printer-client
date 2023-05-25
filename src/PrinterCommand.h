#pragma once

#include <cstdint>

enum PrinterCommand : uint8_t
{
    PrintData = 0xA2,
    Status = 0xA3,
    SetHeat = 0xA4,
    PrintStartStop = 0xA6,
    GetDeviceInfo = 0xA8,
    SetEnergy = 0xAF,
    Draft = 0xBE,
    PaperFeedSpeed = 0xBD,
    PrintDataCompressed = 0xBF,
};
