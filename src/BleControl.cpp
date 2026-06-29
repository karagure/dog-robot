#include "BleControl.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define SERVICE_UUID     "0000ffe0-0000-1000-8000-00805f9b34fb"
#define CHAR_WRITE_UUID  "0000ffe1-0000-1000-8000-00805f9b34fb"
#define CHAR_NOTIFY_UUID "0000ffe2-0000-1000-8000-00805f9b34fb"

static void (*g_onLine)(const char*) = nullptr;
static volatile bool g_connected = false;
static BLECharacteristic* g_notifyChar = nullptr;

class SrvCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer*) override { g_connected = true; }
    void onDisconnect(BLEServer* s) override { g_connected = false; s->startAdvertising(); }
};

class WriteCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* c) override {
        String v = c->getValue().c_str();
        int start = 0;
        for (int i = 0; i <= (int)v.length(); i++) {
            if (i == (int)v.length() || v[i] == '\n') {
                String line = v.substring(start, i);
                line.trim();
                if (line.length() && g_onLine) g_onLine(line.c_str());
                start = i + 1;
            }
        }
    }
};

void BleControl::begin(const char* name, void (*onLine)(const char*)) {
    g_onLine = onLine;
    BLEDevice::init(name);
    BLEServer* server = BLEDevice::createServer();
    server->setCallbacks(new SrvCallbacks());
    BLEService* service = server->createService(SERVICE_UUID);

    BLECharacteristic* writeChar = service->createCharacteristic(
        CHAR_WRITE_UUID,
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
    writeChar->setCallbacks(new WriteCallbacks());

    g_notifyChar = service->createCharacteristic(
        CHAR_NOTIFY_UUID, BLECharacteristic::PROPERTY_NOTIFY);
    g_notifyChar->addDescriptor(new BLE2902());

    service->start();
    BLEAdvertising* adv = BLEDevice::getAdvertising();
    adv->addServiceUUID(SERVICE_UUID);
    adv->setScanResponse(true);
    BLEDevice::startAdvertising();
}

void BleControl::notifyState(const char* msg) {
    if (g_notifyChar && g_connected) {
        g_notifyChar->setValue((uint8_t*)msg, strlen(msg));
        g_notifyChar->notify();
    }
}

bool BleControl::isConnected() { return g_connected; }
