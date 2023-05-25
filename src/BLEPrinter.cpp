#include "BLEPrinter.h"
#include "PrinterPacket.h"
#include "PrinterCommand.h"

BLEPrinter::BLEPrinter(BLEClient* client) :
    printerServiceUUID(BLEUUID((uint16_t)0xae30)),
    writeCharacteristicUUID(BLEUUID((uint16_t)0xae01)),
    notifyCharacteristicUUID(BLEUUID((uint16_t)0xae02)),
    genericAccessServiceUUID(BLEUUID((uint16_t)0x1800)),
    client(client)
{
}

bool BLEPrinter::enableNotifications()
{
    this->getNotifyCharacteristic()
        ->registerForNotify(this->getNotifyCallback(), false);

    return true;
}

void BLEPrinter::requestStatus()
{
    uint8_t statusType = 0x01;
    uint8_t data[PrinterPacket::calculatePacketLength(sizeof(statusType))];

    PrinterPacket::makePacketUInt8(PrinterCommand::Status, statusType, data, sizeof(data));

    this->write(data, sizeof(data));
}

BLERemoteCharacteristic* BLEPrinter::getNotifyCharacteristic()
{
    return this->client
        ->getService(this->printerServiceUUID)
        ->getCharacteristic(this->notifyCharacteristicUUID);
}

BLERemoteCharacteristic* BLEPrinter::getWriteCharacteristic()
{
    return this->client
        ->getService(this->printerServiceUUID)
        ->getCharacteristic(this->writeCharacteristicUUID);
}

void BLEPrinter::notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* data, size_t length, bool isNotify)
{
    if (!isNotify)
        Serial.print("Indication callback for characteristic: ");
    else
        Serial.print("Notification callback for characteristic: ");

    for (int i = 0; i < length; i++)
    {
        Serial.print(data[i], HEX);
        Serial.print(" ");
    }

    Serial.println();

    if (data[2] == PrinterCommand::Status) {
        Serial.println("Status response");

        auto status = data[6];

        if (status == 0x00) {
            this->status = BLEPrinterStatus::Ready;
        }
        else if ((status & (uint8_t)BLEPrinterStatus::Busy) == (uint8_t)BLEPrinterStatus::Busy) {
            this->status = BLEPrinterStatus::Busy;
        } else if ((status & (uint8_t)BLEPrinterStatus::NoPaper) == (uint8_t)BLEPrinterStatus::NoPaper) {
            this->status = BLEPrinterStatus::NoPaper;
        } else if ((status & (uint8_t)BLEPrinterStatus::CoverOpen) == (uint8_t)BLEPrinterStatus::CoverOpen) {
            this->status = BLEPrinterStatus::CoverOpen;
        } else if ((status & (uint8_t)BLEPrinterStatus::Overheat) == (uint8_t)BLEPrinterStatus::Overheat) {
            this->status = BLEPrinterStatus::Overheat;
        } else if ((status & (uint8_t)BLEPrinterStatus::LowPower) == (uint8_t)BLEPrinterStatus::LowPower) {
            this->status = BLEPrinterStatus::LowPower;
        } else {
            this->status = BLEPrinterStatus::Unknown;
        }

        Serial.println("Status: " + this->getStatusString() + " (" + String(status, HEX) + ")");
    }
} 

void BLEPrinter::write(uint8_t* data, size_t length)
{
    this->getWriteCharacteristic()
        ->writeValue(data, length, false);
}

String BLEPrinter::getName()
{
    auto value = this->client
        ->getService(this->genericAccessServiceUUID)
        ->getCharacteristic(BLEUUID((uint16_t)0x2a00))
        ->readValue();

    return String(value.c_str(), value.length());
}

String BLEPrinter::getStatusString()
{
    switch (this->status)
    {
    case BLEPrinterStatus::Ready:
        return "Ready";
    case BLEPrinterStatus::Busy:
        return "Busy";
    case BLEPrinterStatus::NoPaper:
        return "No paper";
    case BLEPrinterStatus::CoverOpen:
        return "Cover open";
    case BLEPrinterStatus::Overheat:
        return "Overheat";
    case BLEPrinterStatus::LowPower:
        return "Low power";
    case BLEPrinterStatus::Unknown:
    default:
        return "Unknown";
    }
}

void BLEPrinter::setHeat(uint8_t heat)
{
    uint8_t data[PrinterPacket::calculatePacketLength(sizeof(heat))];
    PrinterPacket::makePacketUInt8(PrinterCommand::SetHeat, heat, data, sizeof(data));
    this->write(data, sizeof(data));
}

void BLEPrinter::setEnergy(uint16_t energy)
{
    uint8_t data[PrinterPacket::calculatePacketLength(sizeof(energy))];
    uint16_t energyBE = __builtin_bswap16(energy);
    PrinterPacket::makePacket(PrinterCommand::SetEnergy, reinterpret_cast<uint8_t*>(&energyBE), sizeof(energyBE), data, sizeof(data));

    printf("Set energy: %02x%02x\n", (reinterpret_cast<uint8_t*>(&energyBE))[0], (reinterpret_cast<uint8_t*>(&energyBE))[1]);

    this->write(data, sizeof(data));
}

void BLEPrinter::setPaperFeedSpeed(uint8_t speed)
{
    uint8_t data[PrinterPacket::calculatePacketLength(sizeof(speed))];
    PrinterPacket::makePacketUInt8(PrinterCommand::PaperFeedSpeed, speed, data, sizeof(data));
    this->write(data, sizeof(data));
}

void BLEPrinter::setDraft(bool enabled)
{
    uint8_t data[PrinterPacket::calculatePacketLength(sizeof(enabled))];
    PrinterPacket::makePacketUInt8(PrinterCommand::Draft, enabled ? 0x01 : 0x00, data, sizeof(data));
    this->write(data, sizeof(data));
}

void BLEPrinter::printData(uint8_t* data, size_t length)
{
    uint8_t packet[PrinterPacket::calculatePacketLength(length)];
    PrinterPacket::makePacket(PrinterCommand::PrintData, data, length, packet, sizeof(packet));
    this->write(packet, sizeof(packet));
}

void BLEPrinter::getDeviceInfo()
{
    uint8_t data[PrinterPacket::calculatePacketLength(0)];
    PrinterPacket::makePacket(PrinterCommand::GetDeviceInfo, nullptr, 0, data, sizeof(data));
    this->write(data, sizeof(data));


}
