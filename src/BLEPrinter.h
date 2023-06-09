#pragma once

#include <Arduino.h>
#include <BLEDevice.h>

#include "BLEPrinterStatus.h"
#include <StreamingPacketParser.h>

class BLEPrinter
{
    public:
    BLEPrinter(BLEClient* client);

    bool enableNotifications();
    void requestStatus();
    
    BLEPrinterStatus getStatus() { return this->status; }
    String getName();
    String getStatusString();

    void write(uint8_t* data, size_t length);
    void setHeat(uint8_t heat);
    void setEnergy(uint16_t energy);
    void setPaperFeedSpeed(uint8_t speed);
    void setDraft(bool draft);
    void printData(uint8_t* data, size_t length);
    void getDeviceInfo();
    void setPaperDPI(uint16_t dpi);

    StreamingPacketParser* parser;

private:
    BLEClient* client;
    BLEPrinterStatus status = BLEPrinterStatus::Unknown;

    const u_int8_t printLattice[11] = { 0xaa, 0x55, 0x17, 0x38, 0x44, 0x5f, 0x5f, 0x5f, 0x44, 0x38, 0x2c };
    const u_int8_t finishLattice[11] = { 0xaa, 0x55, 0x17, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x17 };

    const BLEUUID printerServiceUUID;
    const BLEUUID writeCharacteristicUUID;
    const BLEUUID notifyCharacteristicUUID;
    const BLEUUID genericAccessServiceUUID;

    BLERemoteCharacteristic* getNotifyCharacteristic();
    BLERemoteCharacteristic* getWriteCharacteristic();

    void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* data, size_t length, bool isNotify);

    notify_callback getNotifyCallback()
    {
        return [this](BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify)
        {
            this->notifyCallback(pBLERemoteCharacteristic, pData, length, isNotify);
        };
    }
};