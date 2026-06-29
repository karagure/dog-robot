#pragma once
#include <Arduino.h>

class BleControl {
public:
    void begin(const char* name, void (*onLine)(const char*));
    void notifyState(const char* msg);
    bool isConnected();
};
