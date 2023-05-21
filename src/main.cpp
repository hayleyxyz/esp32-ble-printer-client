#include <Arduino.h>
#include <BLEDevice.h>
#include <esp_cpu.h>
#include <driver/uart.h>
#include "ApplicationState.h"
#include "BLEPrinter.h"
#include "PrinterPacket.h"

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
    Serial.println("Starting Arduino BLE Client application...");

    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    Serial.println("ESP32 chip info");
    Serial.println("Chip model: " + String(chip_info.model) + " (" + String(ESP.getChipModel()) + ")");
    Serial.println("Chip revision: " + String(chip_info.revision) + " (" + String(ESP.getChipRevision()) + ")");
    Serial.println("Number of cores: " + String(chip_info.cores));
    Serial.println("Features: " + String(chip_info.features));
    Serial.println();
    Serial.println("ESP32 flash info");
    Serial.println("Flash speed: " + String(ESP.getFlashChipSpeed()));
    Serial.println("Flash size: " + String(ESP.getFlashChipSize()));

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
            else if (command == "init") {
                uint8_t setHeat[] = {0x51, 0x78, 0xA4, 0x00, 0x01, 0x00, 0x05, 0x1B, 0xFF,};
                uint8_t setHeatNew[9];
                PrinterPacket::makePacketUInt8(0xA4, 0x05, setHeatNew, sizeof(setHeatNew));

                assert(memcmp(setHeat, setHeatNew, sizeof(setHeat)) == 0);

                printer->write(setHeat, sizeof(setHeat));

                uint8_t setEnergy[] = {0x51, 0x78, 0xAF, 0x00, 0x02, 0x00, 0x80, 0x3E, 0x0C, 0xFF,};
                printer->write(setEnergy, sizeof(setEnergy));

                uint8_t setPaperSpeed[] = {0x51, 0x78, 0xBD, 0x00, 0x01, 0x00, 0x2D, 0xC3, 0xFF,};
                printer->write(setPaperSpeed, sizeof(setPaperSpeed));
            }
            else if (command == "print") {
                uint8_t setDraft[] = {0x51, 0x78, 0xBE, 0x00, 0x01, 0x00, 0x01, 0x07, 0xFF,};
                printer->write(setDraft, sizeof(setDraft));

                for (size_t i = 0; i < 5; i++)
                {
                    uint8_t print[] = {0x51, 0x78, 0xA2, 0x00, 0x30, 0x00, 0xB6, 0x6D, 0xDB, 0xB6, 0x6D, 0xDB, 0xB6, 0x6D, 0xDB, 0xB6, 0x6D, 0xDB, 0xB6, 0x6D, 0xDB, 0xB6, 0x6D, 0xDB, 0xB6, 0x6D, 0xDB, 0xB6, 0x6D, 0xDB, 0xB6, 0x6D, 0xDB, 0xB6, 0x6D, 0xDB, 0xB6, 0x6D, 0xDB, 0xB6, 0x6D, 0xDB, 0xB6, 0x6D, 0xDB, 0xB6, 0x6D, 0xDB, 0xB6, 0x6D, 0xDB, 0xB6, 0x6D, 0xDB, 0x74, 0xFF,};
                    printer->write(print, sizeof(print));
                }
                
            }
            else if (command == "page speed") {
                uint8_t pageSpeed[2] = {0x48, 0x00};
                uint8_t data[PrinterPacket::getPacketLength(sizeof(pageSpeed))];

                PrinterPacket::makePacket(0x5f, pageSpeed, sizeof(pageSpeed), data, sizeof(data));

                printer->write(data, sizeof(data));
            }
            else if(command == "print2") {
                {
                    // Set heat
                    //uint8_t data[] = {0x51, 0x78, 0xA4, 0x00, 0x01, 0x00, 0x33, 0x99, 0xFF,};
                    //printer->write(data, sizeof(data));
                    printer->setHeat(53);
                }
                {
                    // Set heat
                    //uint8_t data[] = {0x51, 0x78, 0xA4, 0x00, 0x01, 0x00, 0x33, 0x99, 0xFF,};
                    //printer->write(data, sizeof(data));
                    printer->setHeat(53);
                }
                {
                    // Set energy
                    uint8_t energyData[2] = {16000 % 0xff, 16000 / 0xff};
                    uint8_t data[PrinterPacket::getPacketLength(sizeof(energyData))] = {0};
                    PrinterPacket::makePacket(0xAF, energyData, sizeof(energyData), data, sizeof(data));

                    printer->write(data, sizeof(data));
                }
                {
                    // Set energy
                    uint8_t energyData[2] = {16000 % 0xff, 16000 / 0xff};
                    uint8_t data[PrinterPacket::getPacketLength(sizeof(energyData))] = {0};
                    PrinterPacket::makePacket(0xAF, energyData, sizeof(energyData), data, sizeof(data));

                    printer->write(data, sizeof(data));
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xA6, 0x00, 0x0B, 0x00, 0xAA, 0x55, 0x17, 0x38, 0x44, 0x5F, 0x5F, 0x5F, 0x44, 0x38, 0x2C, 0xA1, 0xFF,};
                    printer->write(data, sizeof(data));
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xA6, 0x00, 0x0B, 0x00, 0xAA, 0x55, 0x17, 0x38, 0x44, 0x5F, 0x5F, 0x5F, 0x44, 0x38, 0x2C, 0xA1, 0xFF,};
                    printer->write(data, sizeof(data));
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xAF, 0x00, 0x02, 0x00, 0xE0, 0x2E, 0x89, 0xFF,};
                    printer->write(data, sizeof(data));
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xAF, 0x00, 0x02, 0x00, 0xE0, 0x2E, 0x89, 0xFF,};
                    printer->write(data, sizeof(data));
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xBE, 0x00, 0x01, 0x00, 0x00, 0x00, 0xFF,};
                    printer->write(data, sizeof(data));
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xBE, 0x00, 0x01, 0x00, 0x00, 0x00, 0xFF,};
                    printer->write(data, sizeof(data));
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xBD, 0x00, 0x01, 0x00, 0x50, 0xB7, 0xFF,};
                    printer->write(data, sizeof(data));
                }
                {
                    for (size_t i = 0; i < 32*2; i++)
                    {
                        uint8_t data[] = {0x51, 0x78, 0xBD, 0x00, 0x01, 0x00, 0x50, 0xB7, 0xFF,};
                        printer->write(data, sizeof(data));
                    }
                }
                {
                    for (size_t i = 0; i < 32*2; i++)
                    {
                        uint8_t data[] = {0x51, 0x78, 0xA2, 0x00, 0x30, 0x00, 0x97, 0x03, 0x06, 0x51, 0x92, 0xE3, 0xD8, 0xED, 0x09, 0x0B, 0x8A, 0x67, 0x98, 0x61, 0x5A, 0x1F, 0x32, 0x6F, 0x45, 0x85, 0x28, 0x82, 0x06, 0x5F, 0x9A, 0xF8, 0x37, 0x49, 0x5B, 0xF5, 0x8E, 0xA3, 0xD5, 0xB3, 0xBF, 0x06, 0x50, 0x34, 0xFB, 0x7A, 0x60, 0x6E, 0x68, 0x1C, 0x82, 0xAB, 0x93, 0x00, 0x54, 0xFF,};
                        printer->write(data, sizeof(data));
                    }
                    
                    
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xA2, 0x00, 0x30, 0x00, 0x97, 0x03, 0x06, 0x51, 0x92, 0xE3, 0xD8, 0xED, 0x09, 0x0B, 0x8A, 0x67, 0x98, 0x61, 0x5A, 0x1F, 0x32, 0x6F, 0x45, 0x85, 0x28, 0x82, 0x06, 0x5F, 0x9A, 0xF8, 0x37, 0x49, 0x5B, 0xF5, 0x8E, 0xA3, 0xD5, 0xB3, 0xBF, 0x06, 0x50, 0x34, 0xFB, 0x7A, 0x60, 0x6E, 0x68, 0x1C, 0x82, 0xAB, 0x93, 0x00, 0x54, 0xFF,};
                    printer->write(data, sizeof(data));
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xA2, 0x00, 0x30, 0x00, 0x92, 0x64, 0x27, 0x77, 0x77, 0x8D, 0xE5, 0x58, 0x8E, 0xF5, 0xD3, 0xD3, 0xB1, 0xFF, 0x68, 0x61, 0xB7, 0x13, 0xBC, 0x25, 0x65, 0x68, 0x6C, 0x68, 0xE5, 0x8D, 0x59, 0xD8, 0x7A, 0xEB, 0x5C, 0x3F, 0x60, 0x15, 0xC7, 0x66, 0xC6, 0x58, 0x02, 0xD9, 0xD8, 0x4C, 0x63, 0x6D, 0x39, 0x14, 0x94, 0x00, 0xF1, 0xFF,};
                    printer->write(data, sizeof(data));
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xA2, 0x00, 0x30, 0x00, 0xF8, 0xF9, 0x47, 0x88, 0x9D, 0xBC, 0x7D, 0xF6, 0x4F, 0x1D, 0x13, 0x15, 0x41, 0x71, 0x27, 0x8A, 0xB2, 0xD7, 0x14, 0xCE, 0x31, 0x21, 0xE4, 0x5A, 0x9C, 0xED, 0x56, 0x2F, 0xCD, 0xBB, 0xCD, 0x55, 0x1D, 0x6F, 0xC7, 0x40, 0x04, 0x3E, 0x77, 0x75, 0xAE, 0x9B, 0xD2, 0x77, 0xB9, 0xC9, 0x0F, 0x00, 0x7E, 0xFF,};
                    printer->write(data, sizeof(data));
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xA2, 0x00, 0x30, 0x00, 0xEE, 0x70, 0x04, 0xDC, 0x6A, 0x29, 0x2C, 0xC5, 0x2C, 0xF2, 0x78, 0x97, 0x5A, 0x67, 0x9C, 0xDC, 0x81, 0x1F, 0xB0, 0x1D, 0xAE, 0xA1, 0x8E, 0xFB, 0xE1, 0x71, 0xDD, 0x28, 0x4F, 0x4F, 0x0C, 0x53, 0x7B, 0x3F, 0xD0, 0x81, 0xA7, 0xDD, 0x37, 0x63, 0xA3, 0x70, 0xBF, 0x6B, 0x11, 0x33, 0x9D, 0x00, 0xB2, 0xFF,};
                    printer->write(data, sizeof(data));
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xA2, 0x00, 0x30, 0x00, 0x2A, 0x5B, 0x22, 0xF3, 0x05, 0x6C, 0x9D, 0x78, 0x71, 0x6B, 0x56, 0xD4, 0x52, 0x49, 0x6B, 0x8C, 0x43, 0xF8, 0x1D, 0xDE, 0x6B, 0x14, 0x96, 0x27, 0xDC, 0x10, 0x9D, 0x4E, 0xA1, 0xF3, 0x9A, 0x47, 0xC0, 0xDF, 0x40, 0xE2, 0x01, 0xCE, 0x8A, 0xEF, 0x38, 0x0D, 0x7E, 0xFB, 0x20, 0x6A, 0x5E, 0x00, 0xE8, 0xFF,};
                    printer->write(data, sizeof(data));
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xA2, 0x00, 0x30, 0x00, 0x10, 0xA3, 0xDF, 0x13, 0xC9, 0x24, 0xF6, 0x61, 0x3A, 0xF2, 0xE2, 0xA8, 0x5F, 0x56, 0x2F, 0x81, 0xDC, 0xA0, 0x58, 0x3D, 0x86, 0x0B, 0x82, 0xAA, 0x36, 0xB9, 0x2A, 0x44, 0x97, 0xEB, 0x22, 0x25, 0xC0, 0xE4, 0x95, 0x86, 0x26, 0xCC, 0x30, 0x6F, 0x49, 0x6B, 0x8D, 0x51, 0x17, 0x1E, 0xD0, 0x00, 0x3F, 0xFF,};
                    printer->write(data, sizeof(data));
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xA2, 0x00, 0x30, 0x00, 0x13, 0xCB, 0xF3, 0x25, 0x0C, 0x08, 0xE7, 0xBF, 0x74, 0x9E, 0x36, 0x72, 0xB2, 0x5B, 0xAF, 0x9A, 0xA5, 0xDB, 0x2E, 0x0A, 0xAA, 0x1E, 0xEA, 0x5D, 0x79, 0x8D, 0xBD, 0xCD, 0x32, 0xCB, 0x0D, 0x88, 0x7B, 0x98, 0x8D, 0xD8, 0xB2, 0x7B, 0xC7, 0x83, 0xCB, 0x7F, 0x18, 0x8F, 0x4D, 0x29, 0xF4, 0x00, 0x95, 0xFF,};
                    printer->write(data, sizeof(data));
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xA2, 0x00, 0x30, 0x00, 0x52, 0xA8, 0x5C, 0x7E, 0x8E, 0xF6, 0x09, 0xC7, 0x75, 0x36, 0xEE, 0x50, 0xA0, 0xD1, 0x50, 0x0B, 0xFE, 0x54, 0x8F, 0xD8, 0xFA, 0x8E, 0xAA, 0x1E, 0x0A, 0x8C, 0x91, 0xE8, 0x9E, 0x5D, 0x5F, 0xB9, 0x46, 0x90, 0xA6, 0x67, 0x14, 0xA1, 0xB6, 0x29, 0x6B, 0x67, 0x55, 0xE8, 0xB4, 0xE7, 0x75, 0x00, 0x4A, 0xFF,};
                    printer->write(data, sizeof(data));
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xA2, 0x00, 0x30, 0x00, 0x33, 0x13, 0xB9, 0x9D, 0x50, 0x96, 0x46, 0xF2, 0x4C, 0x59, 0x26, 0xA0, 0x50, 0x8E, 0x26, 0x7A, 0xD2, 0x6D, 0x80, 0x0B, 0xAD, 0xBC, 0x75, 0xB2, 0xF5, 0x8D, 0xCB, 0x84, 0x8F, 0x51, 0xC7, 0x65, 0x88, 0xC8, 0x3A, 0xDC, 0xC0, 0x2A, 0xD3, 0xE0, 0x1E, 0x21, 0x9C, 0x7E, 0x41, 0x51, 0x0D, 0x00, 0xED, 0xFF,};
                    printer->write(data, sizeof(data));
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xA2, 0x00, 0x30, 0x00, 0xF6, 0xA0, 0x63, 0x87, 0x89, 0x83, 0x6B, 0x66, 0xA0, 0xC3, 0x2B, 0xD3, 0xC8, 0x34, 0x9D, 0xE2, 0xE2, 0x65, 0xE0, 0x4B, 0x21, 0x67, 0x38, 0x2C, 0xB2, 0xF3, 0x8E, 0xDC, 0x9F, 0x93, 0xE7, 0x62, 0x6A, 0xBD, 0x7E, 0xD0, 0xED, 0x4E, 0x8E, 0x1C, 0x46, 0x1B, 0x45, 0x0D, 0x3B, 0x17, 0x6A, 0x00, 0x1A, 0xFF,};
                    printer->write(data, sizeof(data));
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xA2, 0x00, 0x30, 0x00, 0xA8, 0x6A, 0x9F, 0x47, 0x4C, 0x9F, 0xFE, 0x2C, 0x24, 0xEB, 0xC9, 0x2B, 0xCB, 0xD9, 0x63, 0xC5, 0x64, 0x7B, 0x9F, 0xA3, 0x34, 0x26, 0x77, 0x80, 0xDE, 0x54, 0x94, 0x35, 0xC3, 0x9E, 0xFF, 0x1F, 0x1A, 0x5B, 0x23, 0xD1, 0xE0, 0x47, 0xFD, 0x1B, 0x95, 0x63, 0x9B, 0xF7, 0x93, 0xF4, 0x8D, 0x00, 0xB4, 0xFF,};
                    printer->write(data, sizeof(data));
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xA2, 0x00, 0x30, 0x00, 0xBC, 0x4A, 0x8C, 0x4C, 0x2D, 0x90, 0x9C, 0x4A, 0xD8, 0x0F, 0x6B, 0x17, 0x99, 0xDF, 0xDE, 0xF8, 0x58, 0xB8, 0xF1, 0x7D, 0x41, 0xC8, 0x0A, 0x80, 0xA9, 0x63, 0x95, 0xEF, 0xF3, 0x0A, 0x46, 0xD3, 0xB7, 0x07, 0xF3, 0xBF, 0x26, 0x8B, 0xE9, 0x92, 0x62, 0xCF, 0x2D, 0x1C, 0x32, 0xF8, 0xE5, 0x00, 0x07, 0xFF,};
                    printer->write(data, sizeof(data));
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xA2, 0x00, 0x30, 0x00, 0xBC, 0x9B, 0x9F, 0xAB, 0xBB, 0x8E, 0x69, 0x4C, 0xA6, 0xB0, 0x30, 0x38, 0x93, 0xED, 0x14, 0xFE, 0xDA, 0x8B, 0xE8, 0x07, 0x5A, 0xDA, 0x17, 0xE5, 0xC0, 0x69, 0xFD, 0x47, 0xA4, 0x9C, 0xEA, 0x74, 0x35, 0xE4, 0xFE, 0x61, 0x66, 0x79, 0x7A, 0x89, 0x69, 0x3A, 0x48, 0xBD, 0xC3, 0xD2, 0x0A, 0x00, 0x40, 0xFF,};
                    printer->write(data, sizeof(data));
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xA2, 0x00, 0x30, 0x00, 0x25, 0xA3, 0xD4, 0xAF, 0x7F, 0x1B, 0x4C, 0x2C, 0xCD, 0x9E, 0x6E, 0x94, 0xDD, 0xF7, 0x3F, 0x9B, 0xFC, 0x5B, 0x62, 0x3A, 0x87, 0x97, 0xEC, 0x55, 0xFC, 0xB6, 0xBF, 0xB9, 0xAC, 0x86, 0x60, 0x06, 0xA6, 0x17, 0x9B, 0xFE, 0xB5, 0xE8, 0x61, 0x03, 0x17, 0xB7, 0xB1, 0x4E, 0x34, 0x60, 0x63, 0x00, 0x3D, 0xFF,};
                    printer->write(data, sizeof(data));
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xA2, 0x00, 0x30, 0x00, 0xFA, 0x53, 0x75, 0x1A, 0xA6, 0x36, 0x14, 0xAB, 0xE8, 0xBC, 0x8C, 0x8F, 0x65, 0x2B, 0x7C, 0x04, 0x3E, 0x15, 0x56, 0xD8, 0xC2, 0xD4, 0x87, 0x4D, 0x09, 0x44, 0x84, 0x03, 0xC0, 0xB6, 0x02, 0x6F, 0x16, 0xE4, 0x77, 0xD8, 0x99, 0x8B, 0x87, 0x03, 0xC6, 0x24, 0xA0, 0x23, 0x94, 0x85, 0xEA, 0x00, 0xFC, 0xFF,};
                    printer->write(data, sizeof(data));
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xA2, 0x00, 0x30, 0x00, 0x20, 0x27, 0x58, 0xFA, 0x53, 0x59, 0xC1, 0xC8, 0x2F, 0x86, 0x77, 0x93, 0x0C, 0x81, 0xE3, 0x4F, 0x42, 0xAF, 0x8E, 0xEB, 0x81, 0xD5, 0xFE, 0x16, 0x02, 0x86, 0x37, 0x9E, 0x37, 0x1E, 0xF5, 0x5D, 0xE7, 0x87, 0x24, 0x49, 0x89, 0x6C, 0x42, 0x89, 0x53, 0x21, 0x67, 0xDB, 0xEA, 0x47, 0xB8, 0x00, 0x0C, 0xFF,};
                    printer->write(data, sizeof(data));
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xA2, 0x00, 0x30, 0x00, 0x90, 0x54, 0x05, 0x9F, 0x11, 0x6A, 0x3C, 0x4B, 0x10, 0xBC, 0x9C, 0x94, 0x73, 0xE6, 0xD7, 0x60, 0x9D, 0x60, 0x6B, 0x5D, 0x4D, 0x51, 0x36, 0xAF, 0x17, 0x66, 0x88, 0x29, 0xDC, 0x6E, 0x5A, 0x4E, 0xCD, 0x18, 0x28, 0x0B, 0x82, 0xA7, 0xB1, 0xCF, 0x48, 0x5B, 0x98, 0x46, 0x2B, 0x35, 0x8E, 0x00, 0x4A, 0xFF,};
                    printer->write(data, sizeof(data));
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xA2, 0x00, 0x30, 0x00, 0xBA, 0x66, 0x47, 0x51, 0xA4, 0x86, 0xC0, 0x9F, 0xFF, 0xBD, 0x60, 0x14, 0xF0, 0x0F, 0x77, 0x61, 0xD3, 0x41, 0x42, 0xFC, 0x0E, 0x43, 0x1F, 0x3E, 0x83, 0xCA, 0x86, 0x7C, 0x21, 0x8B, 0x83, 0xB4, 0x80, 0x78, 0xB6, 0xA3, 0xD8, 0x33, 0xA1, 0x96, 0xE9, 0x19, 0x84, 0x22, 0xD5, 0x61, 0x28, 0x00, 0x51, 0xFF,};
                    printer->write(data, sizeof(data));
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xA2, 0x00, 0x30, 0x00, 0x34, 0x36, 0x85, 0x05, 0x35, 0x1F, 0x8B, 0x8E, 0xCD, 0x0C, 0x1C, 0x3F, 0xAA, 0xF9, 0x55, 0xC3, 0xA9, 0xD3, 0x18, 0x64, 0xC9, 0x1A, 0xA6, 0xAD, 0xFB, 0xB6, 0x77, 0xDA, 0xF3, 0xBB, 0x80, 0x0C, 0xC0, 0x2A, 0x38, 0xCD, 0x4D, 0x1A, 0xF6, 0x61, 0x0F, 0x9E, 0x68, 0xD7, 0xC0, 0xF4, 0x83, 0x00, 0x1D, 0xFF,};
                    printer->write(data, sizeof(data));
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xA2, 0x00, 0x30, 0x00, 0xD5, 0x24, 0x45, 0x63, 0xAD, 0x1A, 0xF8, 0x60, 0xB1, 0x17, 0x06, 0x03, 0x2C, 0xEF, 0xB1, 0x64, 0x50, 0x93, 0xD6, 0x42, 0x95, 0x74, 0x11, 0x77, 0x2C, 0x38, 0xBA, 0xF0, 0xF0, 0xA1, 0xC5, 0xDE, 0x89, 0xC3, 0x99, 0x23, 0x21, 0x51, 0x88, 0xAC, 0x79, 0x85, 0xFF, 0x41, 0xDE, 0xF1, 0xE1, 0x00, 0xB3, 0xFF,};
                    printer->write(data, sizeof(data));
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xA2, 0x00, 0x30, 0x00, 0x83, 0xF1, 0xF2, 0xC8, 0xF8, 0xEA, 0xA3, 0x71, 0x58, 0xF9, 0x4E, 0xF6, 0x53, 0xC9, 0x53, 0x2D, 0x6D, 0xDB, 0x38, 0xD2, 0x6B, 0x90, 0x52, 0x02, 0xFB, 0xA4, 0x31, 0xE4, 0x32, 0xD5, 0x39, 0xC6, 0x81, 0x72, 0x68, 0xD5, 0x98, 0x3D, 0xA6, 0x9A, 0x56, 0x84, 0x99, 0x6F, 0x30, 0x29, 0xE1, 0x00, 0x36, 0xFF,};
                    printer->write(data, sizeof(data));
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xA2, 0x00, 0x30, 0x00, 0x21, 0xDD, 0x53, 0xC0, 0x4A, 0xEA, 0x0F, 0x3D, 0x5F, 0x69, 0x3D, 0x00, 0x9E, 0x60, 0x22, 0x46, 0xE1, 0x68, 0x30, 0x2A, 0xC2, 0xB4, 0x63, 0x59, 0x06, 0xA3, 0x66, 0x4B, 0xAA, 0xCA, 0x97, 0x7A, 0x4E, 0x4D, 0xD9, 0x5F, 0xAE, 0x12, 0x15, 0xE9, 0xAD, 0xD9, 0x0D, 0x27, 0x07, 0x6C, 0xD5, 0x00, 0xB8, 0xFF,};
                    printer->write(data, sizeof(data));
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xA2, 0x00, 0x30, 0x00, 0x0F, 0x3E, 0x59, 0x3B, 0x11, 0xD1, 0x6E, 0x7D, 0x01, 0x44, 0x52, 0x06, 0xBA, 0xCF, 0x67, 0x58, 0x31, 0x68, 0xB1, 0xE6, 0x5F, 0xDB, 0x84, 0xB1, 0x3B, 0x4E, 0x15, 0x72, 0x54, 0xB0, 0xB7, 0xC1, 0x50, 0xDD, 0xE6, 0xAE, 0xF5, 0xAC, 0x25, 0x8C, 0x10, 0x89, 0x8A, 0xCE, 0x16, 0x48, 0xCF, 0x00, 0x29, 0xFF,};
                    printer->write(data, sizeof(data));
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xA2, 0x00, 0x30, 0x00, 0xCA, 0x41, 0x6B, 0x19, 0xA0, 0xF5, 0x62, 0xF4, 0xF9, 0x4E, 0xD1, 0x38, 0x8F, 0x97, 0xB6, 0x91, 0x3E, 0xA6, 0xB2, 0x4B, 0x0D, 0x00, 0xBC, 0x8B, 0x2F, 0x72, 0x02, 0x0C, 0x05, 0x62, 0xD2, 0xA9, 0xF3, 0xB0, 0x32, 0x0A, 0xC8, 0x08, 0xC6, 0x90, 0x40, 0x71, 0x35, 0xC2, 0xC1, 0xDF, 0x9C, 0x00, 0xC7, 0xFF,};
                    printer->write(data, sizeof(data));
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xA2, 0x00, 0x30, 0x00, 0x54, 0x14, 0x1C, 0x6E, 0x9A, 0xF5, 0x54, 0x8B, 0x07, 0xEE, 0x2D, 0x69, 0x61, 0x8C, 0xF1, 0x59, 0x3C, 0x8E, 0xB3, 0x50, 0xCA, 0xF9, 0xA7, 0xC7, 0xF8, 0x6C, 0x8D, 0x39, 0x98, 0x6A, 0x2D, 0x99, 0x05, 0xA6, 0xD8, 0x45, 0xAF, 0xE2, 0x1B, 0x75, 0xDD, 0xB7, 0x4F, 0xF7, 0x89, 0xBC, 0x88, 0x00, 0xC7, 0xFF,};
                    printer->write(data, sizeof(data));
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xA2, 0x00, 0x30, 0x00, 0x49, 0x42, 0x33, 0x78, 0x32, 0x37, 0x0B, 0xA9, 0x5F, 0xC0, 0x11, 0x90, 0xE2, 0xDE, 0xFF, 0x60, 0x79, 0x03, 0x02, 0x69, 0x41, 0xD2, 0x61, 0x99, 0x04, 0xAA, 0xA5, 0xAB, 0x21, 0x4B, 0x2F, 0x70, 0x64, 0xDF, 0x5D, 0x24, 0x4E, 0x68, 0xED, 0x6C, 0xC6, 0x5E, 0xE6, 0x3A, 0x4C, 0xE9, 0x24, 0x00, 0xB9, 0xFF,};
                    printer->write(data, sizeof(data));
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xA2, 0x00, 0x30, 0x00, 0x86, 0xB8, 0x22, 0x33, 0xA3, 0x14, 0xF1, 0x7D, 0x22, 0x60, 0x51, 0xA8, 0xCF, 0xF6, 0xAB, 0xE8, 0xBF, 0xB1, 0x43, 0xCA, 0x19, 0xA9, 0x9C, 0x2B, 0x4E, 0x2D, 0x56, 0xE2, 0x40, 0x98, 0x5C, 0x4D, 0xB5, 0x9E, 0x69, 0x54, 0xAA, 0xC8, 0xA7, 0x81, 0xDE, 0xA0, 0xF5, 0x5D, 0xF7, 0x4C, 0xA9, 0x00, 0x7B, 0xFF,};
                    printer->write(data, sizeof(data));
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xA2, 0x00, 0x30, 0x00, 0x8C, 0xF2, 0xD9, 0xA9, 0x9A, 0x6C, 0x6C, 0x18, 0x7C, 0xB8, 0xF9, 0x6D, 0x2F, 0x4A, 0x13, 0x3A, 0x7A, 0x16, 0x72, 0xD2, 0xDA, 0xAA, 0xA6, 0xA6, 0x98, 0xB0, 0x82, 0xAB, 0x9C, 0xC6, 0xC2, 0x13, 0xE2, 0x05, 0x25, 0x40, 0xBD, 0x01, 0xB1, 0x43, 0x7C, 0x2E, 0xE6, 0x41, 0x41, 0xA1, 0xF7, 0x00, 0x28, 0xFF,};
                    printer->write(data, sizeof(data));
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xA2, 0x00, 0x30, 0x00, 0x69, 0xBB, 0x1A, 0x05, 0x89, 0x84, 0xA1, 0xB1, 0x67, 0xA0, 0xD1, 0x84, 0x6E, 0x84, 0x0B, 0x22, 0xD6, 0xFF, 0xDA, 0x37, 0x4B, 0x67, 0xAF, 0x9E, 0x62, 0x19, 0x8A, 0xA7, 0x0A, 0xB4, 0x1F, 0xCA, 0x6D, 0xDD, 0xB1, 0x1F, 0x7A, 0xF5, 0x6C, 0x7D, 0xC5, 0x96, 0x45, 0x77, 0x69, 0x4A, 0xA2, 0x00, 0x05, 0xFF,};
                    printer->write(data, sizeof(data));
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xA2, 0x00, 0x30, 0x00, 0x1E, 0x07, 0x8F, 0xE0, 0xC2, 0x9C, 0xB1, 0xD1, 0x9B, 0x6C, 0xFC, 0x5B, 0x65, 0x67, 0xD1, 0xDA, 0x47, 0xA3, 0x7A, 0xC0, 0x6C, 0xD5, 0x40, 0x62, 0xCA, 0x4C, 0x56, 0x58, 0xBF, 0x9F, 0xE8, 0x78, 0xF5, 0x9A, 0x7C, 0x2C, 0x0C, 0x02, 0x45, 0xE9, 0xD9, 0xE8, 0x46, 0x74, 0x5C, 0x16, 0x0D, 0x00, 0x7E, 0xFF,};
                    printer->write(data, sizeof(data));
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xA2, 0x00, 0x30, 0x00, 0xD2, 0x96, 0x16, 0x27, 0x28, 0x5D, 0xBA, 0x7A, 0x54, 0x45, 0x55, 0xBF, 0xBB, 0x52, 0x0C, 0x8D, 0x50, 0x21, 0x3A, 0x39, 0xE4, 0xF0, 0x47, 0x87, 0x11, 0x12, 0xA8, 0x68, 0xD4, 0xE2, 0x41, 0x89, 0x06, 0x24, 0xC1, 0xB5, 0x72, 0x64, 0x30, 0x85, 0x38, 0x51, 0xDA, 0x52, 0x9E, 0x43, 0x7F, 0x00, 0xEE, 0xFF,};
                    printer->write(data, sizeof(data));
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xA2, 0x00, 0x30, 0x00, 0xEA, 0xEC, 0xC0, 0xDD, 0x4B, 0xA1, 0xDE, 0x76, 0x7D, 0x65, 0x4A, 0xA5, 0x80, 0x66, 0xED, 0x19, 0x02, 0xA6, 0x63, 0x85, 0xB4, 0xFE, 0xFB, 0x32, 0xE1, 0x22, 0x5C, 0x09, 0xC9, 0x91, 0x6E, 0xF7, 0x78, 0x37, 0x37, 0x06, 0xDE, 0x2D, 0x78, 0xC4, 0xD5, 0x0B, 0xF3, 0x12, 0x8B, 0x22, 0x24, 0x00, 0xA9, 0xFF,};
                    printer->write(data, sizeof(data));
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xA2, 0x00, 0x30, 0x00, 0x84, 0x86, 0xC1, 0xA9, 0xCB, 0x42, 0x66, 0xED, 0x2F, 0x9F, 0xA5, 0x2B, 0x74, 0x06, 0x86, 0x8B, 0xE0, 0x5C, 0x42, 0xCD, 0x8E, 0x3F, 0x75, 0xD6, 0xB6, 0x39, 0x1D, 0xC3, 0x96, 0x10, 0x4B, 0xF4, 0xDB, 0x07, 0x8D, 0x76, 0x08, 0xB9, 0x93, 0x31, 0x9C, 0x64, 0xC0, 0x54, 0x91, 0x57, 0x0F, 0x00, 0xA2, 0xFF,};
                    printer->write(data, sizeof(data));
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xA2, 0x00, 0x30, 0x00, 0xEE, 0x21, 0xDD, 0x3B, 0x33, 0x73, 0xB0, 0x25, 0x3C, 0x5B, 0xA3, 0xAC, 0xFC, 0x71, 0x62, 0xC1, 0xC1, 0x5C, 0xA5, 0x3F, 0xC1, 0x25, 0x97, 0x21, 0xF4, 0xE5, 0x63, 0x81, 0x52, 0x21, 0xF6, 0x7D, 0xB5, 0x76, 0x50, 0xD1, 0x92, 0xB4, 0xC5, 0x50, 0xBB, 0x22, 0xF8, 0x44, 0x6B, 0x28, 0x43, 0x00, 0x55, 0xFF,};
                    printer->write(data, sizeof(data));
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xA2, 0x00, 0x30, 0x00, 0x0B, 0x74, 0xB8, 0xF8, 0xDF, 0xEF, 0x6B, 0x99, 0xED, 0x91, 0x6F, 0xF7, 0x6C, 0xCC, 0xB7, 0x61, 0xC5, 0xD6, 0x6C, 0x8E, 0x2F, 0xD6, 0x02, 0xF2, 0x41, 0x01, 0x9B, 0xA7, 0x5B, 0xB1, 0xB9, 0x90, 0x01, 0x3E, 0x00, 0xF2, 0xCD, 0x12, 0xB6, 0xC9, 0xFE, 0xB2, 0x9A, 0x10, 0xF3, 0xCA, 0x4F, 0x00, 0x09, 0xFF,};
                    printer->write(data, sizeof(data));
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xA2, 0x00, 0x30, 0x00, 0xD8, 0x33, 0x89, 0xD8, 0x17, 0xB0, 0x8F, 0xB5, 0x86, 0xF1, 0xC9, 0x2B, 0x73, 0x32, 0x75, 0xE1, 0x22, 0x37, 0xB0, 0x0A, 0x7E, 0x90, 0x5B, 0x78, 0xCE, 0x87, 0x73, 0x63, 0x2E, 0x07, 0xB7, 0x83, 0xF4, 0x07, 0x47, 0x05, 0x49, 0x41, 0xBE, 0x4E, 0x20, 0x2B, 0x0C, 0x5F, 0x3B, 0x9B, 0x75, 0x00, 0x54, 0xFF,};
                    printer->write(data, sizeof(data));
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xA2, 0x00, 0x30, 0x00, 0x0D, 0x63, 0x3C, 0x77, 0xE2, 0xCB, 0xA9, 0x80, 0xA7, 0xB1, 0xB7, 0x70, 0xDE, 0x51, 0x37, 0x76, 0xDC, 0xAB, 0x96, 0xA2, 0x91, 0xCF, 0x73, 0xE6, 0xB0, 0xF6, 0x56, 0x52, 0x81, 0xEC, 0x63, 0x66, 0xE3, 0xEA, 0xAE, 0x59, 0x9B, 0x8C, 0x52, 0x1C, 0xEA, 0xE6, 0x81, 0x19, 0x5F, 0x0B, 0xE4, 0x00, 0xDB, 0xFF,};
                    printer->write(data, sizeof(data));
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xA2, 0x00, 0x30, 0x00, 0xAF, 0xC6, 0xF1, 0x8F, 0xA8, 0x7D, 0x1C, 0xE6, 0x1C, 0xE0, 0xAC, 0xD8, 0xBB, 0xFA, 0xF3, 0xC4, 0x7E, 0x64, 0x0C, 0x86, 0xA1, 0xA4, 0x87, 0xF4, 0xD2, 0xED, 0x22, 0x28, 0xA1, 0x9E, 0x68, 0x3D, 0x32, 0x10, 0x92, 0x77, 0xAC, 0x46, 0xEA, 0xEF, 0x79, 0x96, 0x82, 0x00, 0x91, 0x2E, 0xE1, 0x00, 0xB3, 0xFF,};
                    printer->write(data, sizeof(data));
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xA2, 0x00, 0x30, 0x00, 0x43, 0xAA, 0xFE, 0x43, 0x03, 0xD5, 0xA3, 0x5B, 0xF1, 0x35, 0x5B, 0xFE, 0xAB, 0x94, 0x73, 0x94, 0x85, 0x74, 0xD8, 0xDA, 0x54, 0x8E, 0xCE, 0xBB, 0x3C, 0x71, 0x4D, 0xDA, 0x43, 0x32, 0xDD, 0x40, 0x03, 0x91, 0xC6, 0xBE, 0x59, 0x71, 0x36, 0x45, 0xCE, 0x10, 0x92, 0xBB, 0xB1, 0xEF, 0xCD, 0x00, 0x37, 0xFF,};
                    printer->write(data, sizeof(data));
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xA2, 0x00, 0x30, 0x00, 0xB2, 0xC2, 0x55, 0x7C, 0x3E, 0xAD, 0x62, 0x06, 0xB6, 0x6B, 0x9C, 0xED, 0x4B, 0xCC, 0x2C, 0x9C, 0x07, 0xEF, 0x47, 0x14, 0xAA, 0x3B, 0x94, 0x96, 0x6C, 0x27, 0xEC, 0x37, 0x1E, 0xE1, 0xCA, 0x35, 0x8F, 0x04, 0x05, 0x54, 0x7A, 0xC7, 0x93, 0xD0, 0x5C, 0x4E, 0x89, 0xC2, 0x6F, 0xF4, 0x67, 0x00, 0xFC, 0xFF,};
                    printer->write(data, sizeof(data));
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xA2, 0x00, 0x30, 0x00, 0x73, 0x90, 0x3A, 0x53, 0x25, 0xB9, 0x8E, 0x45, 0x6D, 0x35, 0xD1, 0xCA, 0xE4, 0x91, 0x11, 0x98, 0xC6, 0x60, 0xD1, 0xE2, 0x0E, 0x5D, 0x2F, 0x2A, 0x2C, 0x38, 0xDC, 0x1E, 0x4C, 0x3B, 0x49, 0x51, 0xCB, 0xEB, 0xC4, 0xAB, 0x2A, 0x2D, 0x7C, 0x39, 0x8D, 0x75, 0x16, 0xB2, 0x76, 0x4F, 0xC5, 0x00, 0x68, 0xFF,};
                    printer->write(data, sizeof(data));
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xA2, 0x00, 0x30, 0x00, 0x0D, 0x0E, 0x37, 0x30, 0x5D, 0x51, 0x45, 0x7C, 0x7C, 0x18, 0x0C, 0xC0, 0x70, 0x74, 0x8A, 0x96, 0xCA, 0x75, 0xF0, 0x14, 0xFA, 0x9B, 0xA1, 0xAD, 0x16, 0x73, 0xE9, 0x08, 0x9C, 0x59, 0xE3, 0x41, 0xE3, 0xC8, 0xA3, 0x18, 0xB6, 0x98, 0xA0, 0xB9, 0xC6, 0xC6, 0x1D, 0xB9, 0xE1, 0xBC, 0xEB, 0x00, 0xD8, 0xFF,};
                    printer->write(data, sizeof(data));
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xA2, 0x00, 0x30, 0x00, 0x66, 0xA5, 0xCE, 0xCE, 0xD9, 0xE5, 0xDA, 0xA9, 0xDC, 0x61, 0x90, 0xEB, 0x31, 0x34, 0x3B, 0xB2, 0x72, 0x36, 0xFC, 0x52, 0x45, 0x28, 0xE3, 0x27, 0x0C, 0x74, 0x57, 0x4A, 0x88, 0x14, 0xF4, 0xC7, 0x11, 0xC3, 0xA6, 0x0B, 0x34, 0x97, 0xFE, 0xF6, 0x10, 0x49, 0x82, 0x5B, 0x4A, 0x6F, 0x76, 0x00, 0xE1, 0xFF,};
                    printer->write(data, sizeof(data));
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xA2, 0x00, 0x30, 0x00, 0xC1, 0x7D, 0xF8, 0x9E, 0x91, 0x20, 0xE8, 0x3F, 0x87, 0x30, 0xB7, 0xAF, 0xF0, 0x3F, 0x53, 0x0E, 0x6F, 0x40, 0x7C, 0x09, 0x63, 0x46, 0xF9, 0x97, 0xB3, 0x79, 0x3C, 0x67, 0x7D, 0x8F, 0xFE, 0xF0, 0xB5, 0x55, 0x79, 0xF5, 0x0C, 0xE3, 0x52, 0x8D, 0xE0, 0xB6, 0x07, 0x1D, 0xBF, 0x0A, 0x30, 0x00, 0xF7, 0xFF,};
                    printer->write(data, sizeof(data));
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xA2, 0x00, 0x30, 0x00, 0xDC, 0x7C, 0x27, 0xE8, 0x95, 0x0D, 0x0E, 0x5E, 0x4A, 0x7B, 0x70, 0xC3, 0xEE, 0xCE, 0x9E, 0x2A, 0xCF, 0x72, 0x28, 0x17, 0x8D, 0xE5, 0x7C, 0x7F, 0x68, 0xE9, 0x25, 0x14, 0x20, 0xA8, 0xAF, 0x86, 0x56, 0x40, 0x47, 0xB0, 0x9E, 0x42, 0xE3, 0x89, 0xFB, 0x5D, 0x88, 0x0F, 0xE4, 0x61, 0x76, 0x00, 0xDB, 0xFF,};
                    printer->write(data, sizeof(data));
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xA2, 0x00, 0x30, 0x00, 0xA7, 0x63, 0xD8, 0xB8, 0x83, 0x2F, 0xA5, 0x93, 0xD1, 0x14, 0x86, 0xB6, 0x24, 0x16, 0x9C, 0xE1, 0xED, 0xAC, 0x3D, 0x3F, 0x66, 0x1E, 0xDC, 0xD1, 0x25, 0xDC, 0xAD, 0xBA, 0x60, 0xD3, 0x73, 0xD0, 0xFE, 0xD1, 0x41, 0xBB, 0xA4, 0x57, 0x56, 0x30, 0x1C, 0x27, 0x2D, 0x5E, 0xE1, 0x08, 0x38, 0x00, 0xE8, 0xFF,};
                    printer->write(data, sizeof(data));
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xA2, 0x00, 0x30, 0x00, 0x2A, 0xF5, 0xD9, 0x62, 0xE7, 0x42, 0x51, 0xB5, 0x24, 0x09, 0x01, 0x9C, 0x08, 0xF7, 0xC3, 0x0F, 0x4B, 0x31, 0x1B, 0xB1, 0xAF, 0x95, 0x7B, 0x00, 0x4A, 0xDE, 0x3D, 0x56, 0xD9, 0xA1, 0x26, 0x43, 0xF6, 0x9B, 0xA8, 0x9A, 0x97, 0x4A, 0x8D, 0x41, 0x10, 0x2A, 0x1F, 0x01, 0x16, 0x34, 0x7D, 0x00, 0xA1, 0xFF,};
                    printer->write(data, sizeof(data));
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xA2, 0x00, 0x30, 0x00, 0xB6, 0x1A, 0xC4, 0xA0, 0x49, 0xC2, 0x60, 0x08, 0xEF, 0x39, 0x2A, 0x77, 0x2F, 0x2B, 0x82, 0xE1, 0xD8, 0x28, 0xC3, 0x64, 0xE4, 0xC9, 0x5C, 0xA8, 0x76, 0xA8, 0x09, 0x01, 0x1C, 0x42, 0x1E, 0xED, 0xA1, 0xCC, 0xCE, 0x6E, 0x52, 0x93, 0x88, 0x94, 0xE1, 0x83, 0x10, 0xCB, 0xF2, 0x57, 0x37, 0x00, 0x1B, 0xFF,};
                    printer->write(data, sizeof(data));
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xA2, 0x00, 0x30, 0x00, 0x99, 0x22, 0x9D, 0xB9, 0xBF, 0x4E, 0x00, 0x6B, 0x44, 0x27, 0x73, 0x24, 0x7D, 0x64, 0x17, 0x78, 0x01, 0x91, 0x28, 0x85, 0xB2, 0x73, 0x66, 0xD8, 0xD2, 0x62, 0x84, 0xAF, 0xD3, 0x7C, 0xBA, 0xF2, 0x08, 0x0F, 0x6F, 0x31, 0xDC, 0x6C, 0x2E, 0x1C, 0x6E, 0x6A, 0x4E, 0x8E, 0x48, 0xBC, 0xA2, 0x00, 0xE2, 0xFF,};
                    printer->write(data, sizeof(data));
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xA2, 0x00, 0x30, 0x00, 0x99, 0x22, 0x9D, 0xB9, 0xBF, 0x4E, 0x00, 0x6B, 0x44, 0x27, 0x73, 0x24, 0x7D, 0x64, 0x17, 0x78, 0x01, 0x91, 0x28, 0x85, 0xB2, 0x73, 0x66, 0xD8, 0xD2, 0x62, 0x84, 0xAF, 0xD3, 0x7C, 0xBA, 0xF2, 0x08, 0x0F, 0x6F, 0x31, 0xDC, 0x6C, 0x2E, 0x1C, 0x6E, 0x6A, 0x4E, 0x8E, 0x48, 0xBC, 0xA2, 0x00, 0xE2, 0xFF,};
                    printer->write(data, sizeof(data));
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xBD, 0x00, 0x01, 0x00, 0x19, 0x4F, 0xFF,};
                    printer->write(data, sizeof(data));
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xA1, 0x00, 0x02, 0x00, 0x30, 0x00, 0xF9, 0xFF,};
                    //printer->write(data, sizeof(data));
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xA1, 0x00, 0x02, 0x00, 0x30, 0x00, 0xF9, 0xFF,};
                    //printer->write(data, sizeof(data));

                    for (int i = 0; i < sizeof(data); i++)
                    {
                        Serial.print(data[i], HEX);
                        Serial.print(" ");
                    }

                    Serial.println();
                }
                {
                    // scroll?
                    uint8_t scrollData[2] = {0x30, 0x00};
                    uint8_t data[PrinterPacket::getPacketLength(sizeof(scrollData))] = {0};
                    PrinterPacket::makePacket(0xA1, scrollData, sizeof(scrollData), data, sizeof(data));

                    for (int i = 0; i < sizeof(data); i++)
                    {
                        Serial.print(data[i], HEX);
                        Serial.print(" ");
                    }

                    Serial.println();

                    printer->write(data, sizeof(data));
                }
                {
                    // scroll?
                    uint8_t scrollData[2] = {0x30, 0x00};
                    uint8_t data[PrinterPacket::getPacketLength(sizeof(scrollData))] = {0};
                    PrinterPacket::makePacket(0xA1, scrollData, sizeof(scrollData), data, sizeof(data));

                    for (int i = 0; i < sizeof(data); i++)
                    {
                        Serial.print(data[i], HEX);
                        Serial.print(" ");
                    }

                    Serial.println();

                    printer->write(data, sizeof(data));
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xA6, 0x00, 0x0B, 0x00, 0xAA, 0x55, 0x17, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x17, 0x11, 0xFF,};
                    printer->write(data, sizeof(data));
                }
                {
                    uint8_t data[] = {0x51, 0x78, 0xA3, 0x00, 0x01, 0x00, 0x00, 0x00, 0xFF,};
                    printer->write(data, sizeof(data));
                }
            }
            else if (command == "disconnect") {
                bclient->disconnect();
            }

            break;
        }
    }
}
