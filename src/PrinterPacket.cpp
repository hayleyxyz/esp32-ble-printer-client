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
    header.magic = 0x7851;
    header.command = command;
    header.direction = 0x00;
    header.length.length8.low = dataLength & 0xff;
    header.length.length8.high = (dataLength >> 8) & 0xff;

    uint8_t checksum = Checksum8Bit::calculate(data, dataLength);

    PacketFooter footer;
    footer.checksum = checksum;
    footer.end = 0xff;

    memcpy(dst, &header, sizeof(header));
    memcpy(dst + sizeof(header), data, dataLength);
    memcpy(dst + sizeof(header) + dataLength, &footer, sizeof(footer));
}

void PrinterPacket::makePacketUInt8(uint8_t command, uint8_t data, uint8_t *dst, size_t dstLength)
{
    makePacket(command, &data, sizeof(data), dst, dstLength);
}