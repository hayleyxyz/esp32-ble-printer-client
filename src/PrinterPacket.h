#pragma once

#include <cstdlib>

class PrinterPacket
{
public:
    static void makePacket(uint8_t command, uint8_t *data, size_t dataLength, uint8_t *dst, size_t dstLength);
    static void makePacketUInt8(uint8_t command, uint8_t data, uint8_t *dst, size_t dstLength);
    constexpr static size_t getPacketLength(size_t dataLength)
    {
        return sizeof(PacketHeader) + dataLength + sizeof(PacketFooter);
    }

private:
    struct PacketHeader
    {
        uint16_t magic;
        uint8_t command;
        uint8_t direction;
        union {
            uint16_t length16;
            struct {
                uint8_t low;
                uint8_t high;
            } length8;
        } length;
    };

    struct PacketFooter
    {
        uint8_t checksum;
        uint8_t end;
    };
};