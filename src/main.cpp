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
    bclient->setMTU(59);
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
                
            }

            break;
        }
    }
}
