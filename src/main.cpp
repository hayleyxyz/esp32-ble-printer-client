#include <Arduino.h>
#include <BLEDevice.h>
#include <esp_cpu.h>
#include <driver/uart.h>
#include <esp_task_wdt.h>
#include "ApplicationState.h"
#include "BLEPrinter.h"
#include <PrinterPacket.h>


class ApplicationBLECharacteristicCallbacks;

static BLEUUID advertisedServiceUUID(BLEUUID((uint16_t)0xaf30));
static BLEUUID genericAccessServiceUUID(BLEUUID((uint16_t)0x1800));
static BLEUUID printerServiceUUID(BLEUUID((uint16_t)0xae30));
static BLEUUID writeCharacteristicUUID(BLEUUID((uint16_t)0xae01));
static BLEUUID notifyCharacteristicUUID(BLEUUID((uint16_t)0xae02));

static BLEPrinter *printer = nullptr;;

static BLEAddress *pServerAddress = nullptr;;
static BLEClient *bclient = nullptr;
static BLERemoteCharacteristic *pRemoteCharacteristic = nullptr;;

static ApplicationState applicationState = ApplicationState::Initializing;

class ApplicationBLEAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
    void onResult(BLEAdvertisedDevice advertisedDevice)
    {
        Serial.print("BLE Advertised Device found: ");
        Serial.println(advertisedDevice.toString().c_str());

        if (advertisedDevice.isAdvertisingService(advertisedServiceUUID))
        {
            advertisedDevice.getScan()->stop();
            pServerAddress = new BLEAddress(advertisedDevice.getAddress());

            Serial.println("Found our device!");
        }
    }
};

class ApplicationClientCallbacks : public BLEClientCallbacks
{
    void onConnect(BLEClient *bclient)
    {
        Serial.println("onConnect");

        applicationState = ApplicationState::Connected;
    }

    void onDisconnect(BLEClient *bclient)
    {
        Serial.println("onDisconnect");

        applicationState = ApplicationState::Restarting;
    }
};

void setup()
{
    Serial.begin(115200);

    printf("ESP32 Printer Client\n");

    BLEDevice::init("ESP32");
    bclient = BLEDevice::createClient();
    bclient->setMTU(512);
    bclient->setClientCallbacks(new ApplicationClientCallbacks());
}

String readCommandFromSerial()
{
    String command;

    while(true) {
        char c = Serial.read();

        if (c == 0xff || c == -1) {
            continue;
        }

        Serial.print(c);

        if (c == '\r') {
            continue;
        }

        if (c == '\n') {
            break;
        }

        command += c;
    }

    return command;
}

void loop()
{
    switch(applicationState)
    {
        case ApplicationState::Restarting:
            Serial.print("Restarting in... ");

            for (int i = 3; i > 0; i--)
            {
                Serial.print(i);
                Serial.print(" ");
                delay(1000);
            }

            esp_restart();

        case ApplicationState::Initializing:
        {
            pServerAddress = nullptr;
            BLEScan *pBLEScan = BLEDevice::getScan();
            pBLEScan->setAdvertisedDeviceCallbacks(new ApplicationBLEAdvertisedDeviceCallbacks());
            pBLEScan->setActiveScan(true);
            pBLEScan->start(10, true);

            applicationState = ApplicationState::Scanning;

            break;
        }

        case ApplicationState::Scanning:
        {
            if (pServerAddress != nullptr)
            {
                Serial.print("Server address: ");
                Serial.println(pServerAddress->toString().c_str());

                applicationState = ApplicationState::Connecting;
                bclient->connect(*pServerAddress);
            }

            break;
        }

        case ApplicationState::Connecting:
        {
            Serial.println("Connecting...");
            break;
        }

        case ApplicationState::Connected:
        {
            Serial.println("Connected");

            printer = new BLEPrinter(bclient);

            if (!printer->enableNotifications())
            {
                Serial.println("Failed to register for notifications");

                bclient->disconnect();
                pServerAddress = nullptr;
            }

            printer->requestStatus();

            applicationState = ApplicationState::Idle;

            break;
        }

        case ApplicationState::Idle:
        {
            Serial.print("> ");

            String command = readCommandFromSerial();

            if (command == "status")
            {
                Serial.println("Requesting status...");
                printer->requestStatus();
            }
            else if (command == "name") {
                Serial.println("Name: " + printer->getName());
            }
            else if (command == "devinfo") {
                printer->getDeviceInfo();
            }
            else if (command == "disconnect") {
                bclient->disconnect();
            }
            else if (command == "print") {
                printer->getDeviceInfo();
                printer->requestStatus();
                
                //Command: (f2) Unknown | 01 82 
                uint8_t unkData[2] = {0x01, 0x82};
                uint8_t unkPacketData[PrinterPacket::calculatePacketLength(sizeof(unkData))];
                PrinterPacket::makePacket(0xf2, unkData, sizeof(unkData), unkPacketData, sizeof(unkPacketData));
                printer->write(unkPacketData, sizeof(unkPacketData));
                
                printer->requestStatus();

                // Command: (a4) SetHeat | 34
                printer->setHeat(0x34);

                //Command: (a6) PrintStartStop | aa 55 17 38 44 5f 5f 5f 44 38 2c 
                uint8_t printStartData[] = {0xaa, 0x55, 0x17, 0x38, 0x44, 0x5f, 0x5f, 0x5f, 0x44, 0x38, 0x2c};
                uint8_t packetprintStartData[PrinterPacket::calculatePacketLength(sizeof(printStartData))];
                PrinterPacket::makePacket(0xa6, printStartData, sizeof(printStartData), packetprintStartData, sizeof(packetprintStartData));
                printer->write(packetprintStartData, sizeof(packetprintStartData));

                //Command: (af) SetEnergy | 98 3a
                printer->setEnergy(0x3a98);

                printer->setPaperDPI(0x0030);

                // Command: (be) Draft | 00
                printer->setDraft(0);

                //Command: (bd) PaperFeedSpeed | 0a
                printer->setPaperFeedSpeed(0x0a);

                uint8_t* printData = new uint8_t[0x30];
                uint8_t printDataPacket[PrinterPacket::calculatePacketLength(0x30)];

                for (size_t i = 0; i < 30; i++)
                {
                    memset(printData, 0x1c, 0x30);

                    switch (i)
                    {
                        case 21:
                            for (size_t i = 0; i < 16; i++) printData[i] = 0x1c;
                            break;
                        case 22:
                            for (size_t i = 0; i < 16; i++) printData[i] = 0x3e;
                            break;
                        case 23:
                            for (size_t i = 0; i < 16; i++) printData[i] = 0x3e;
                            break;
                        case 24:
                            for (size_t i = 0; i < 16; i++) printData[i] = 0xce;
                            break;
                        default:
                            break;
                    }

                    PrinterPacket::makePacket(0xa2, printData, 0x30, printDataPacket, sizeof(printDataPacket));
                    printer->write(printDataPacket, sizeof(printDataPacket));
                    
                }

                delete[] printData;

                // Command: (bd) PaperFeedSpeed | 19
                printer->setPaperFeedSpeed(0x19);

                //Command: (a6) PrintStartStop | aa 55 17 00 00 00 00 00 00 00 17  
                uint8_t printStopData[] = {0xaa, 0x55, 0x17, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x17};
                uint8_t packetprintStopData[PrinterPacket::calculatePacketLength(sizeof(printStopData))];
                PrinterPacket::makePacket(0xa6, printStopData, sizeof(printStopData), packetprintStopData, sizeof(packetprintStopData));
                printer->write(packetprintStopData, sizeof(packetprintStopData));

            }

            break;
        }
    }
}
