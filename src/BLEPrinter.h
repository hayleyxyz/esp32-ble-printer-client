#pragma once

#include <Arduino.h>
#include <BLEDevice.h>

#include "BLEPrinterStatus.h"

class BLEPrinter
{
    public:
    BLEPrinter(BLEClient* client);

    bool enableNotifications();
    bool requestStatus();
    
    BLEPrinterStatus getStatus() { return this->status; }
    String getName();
    String getStatusString();

    void write(uint8_t* data, size_t length);
    void setHeat(uint8_t heat);

    

private:
    BLEClient* client;
    BLEPrinterStatus status = BLEPrinterStatus::Unknown;

    const BLEUUID printerServiceUUID;
    const BLEUUID writeCharacteristicUUID;
    const BLEUUID notifyCharacteristicUUID;
    const BLEUUID genericAccessServiceUUID;

    const uint8_t CommandOnOff = 0x52;
    const uint8_t CommandStatus = 0xA3;
    const uint8_t CommandSetHeat = 0xA4;
    const u_int8_t CommandSetEnergy = 0xAF;

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