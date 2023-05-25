#include <cstring>
#include <algorithm>
#include <iostream>
#include "PrinterPacket.h"
#include "Checksum8Bit.h"

#include <Arduino.h>

void PrinterPacket::makePacket(uint8_t command, uint8_t *data, size_t dataLength, uint8_t *dst, size_t dstLength)
{
    assert(dstLength >= sizeof(PacketHeader) + dataLength + sizeof(PacketFooter));

    PacketHeader header;
    header.magic = PrinterPacket::Magic;
    header.command = command;
    header.direction = 0x00;
    header.length = dataLength;

    uint8_t checksum = Checksum8Bit::calculate(data, dataLength);

    PacketFooter footer;
    footer.checksum = checksum;
    footer.end = PrinterPacket::End;

    memcpy(dst, &header, sizeof(header));
    memcpy(dst + sizeof(header), data, dataLength);
    memcpy(dst + sizeof(header) + dataLength, &footer, sizeof(footer));
}

void PrinterPacket::makePacketUInt8(uint8_t command, uint8_t data, uint8_t *dst, size_t dstLength)
{
    makePacket(command, &data, sizeof(data), dst, dstLength);
}

bool PrinterPacket::dissectPacket(uint8_t *packet, size_t packetLength, PacketHeader** header, uint8_t **outData, PacketFooter** footer)
{
    if (packetLength < sizeof(PacketHeader))
    {
        return false;
    }

    PacketHeader* headerPtr = reinterpret_cast<PacketHeader*>(packet);

    if (headerPtr->magic != 0x7851)
    {
        return false;
    }

    if (header != nullptr)
    {
        *header = headerPtr;
    }

    if (outData != nullptr)
    {
        *outData = packet + sizeof(PacketHeader);
    }

    if (footer != nullptr && packetLength >= sizeof(PacketHeader) + headerPtr->length + sizeof(PacketFooter))
    {
        *footer = reinterpret_cast<PacketFooter*>(packet + sizeof(PacketHeader) + headerPtr->length);
    }

    return true;
}
