#pragma once

#include <cstdlib>

struct PacketHeader
    {
        uint16_t magic;
        uint8_t command;
        uint8_t direction;
        uint16_t length;
    };

    struct PacketFooter
    {
        uint8_t checksum;
        uint8_t end;
    };

class PrinterPacket
{
public:
    static void makePacket(uint8_t command, uint8_t *data, size_t dataLength, uint8_t *dst, size_t dstLength);
    static void makePacketUInt8(uint8_t command, uint8_t data, uint8_t *dst, size_t dstLength);
    static bool dissectPacket(uint8_t *packet, size_t packetLength, PacketHeader** header, uint8_t **outData, PacketFooter** footer);
    constexpr static size_t calculatePacketLength(size_t dataLength)
    {
        return sizeof(PacketHeader) + dataLength + sizeof(PacketFooter);
    }

    static const uint16_t Magic = 0x7851;
    static const uint8_t End = 0xff;
};