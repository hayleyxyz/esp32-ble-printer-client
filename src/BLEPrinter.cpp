#include "BLEPrinter.h"
#include "PrinterPacket.h"

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

bool BLEPrinter::requestStatus()
{
    uint8_t statusType = 0x01;
    uint8_t data[9];

    PrinterPacket::makePacketUInt8(CommandStatus, statusType, data, sizeof(data));

    this->getWriteCharacteristic()
        ->writeValue(data, sizeof(data), false);

    return true;
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

    if (data[2] == CommandStatus) {
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
    uint8_t data[PrinterPacket::getPacketLength(sizeof(heat))];
    PrinterPacket::makePacketUInt8(CommandSetHeat, heat, data, sizeof(data));
    this->write(data, sizeof(data));
}