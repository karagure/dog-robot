#pragma once
#include <Arduino.h>

// Capteur DHT11 (température + humidité). Lecture non-bloquante.
class EnvSensor {
public:
    void begin();
    bool update();                       // true si une nouvelle lecture valide vient d'être faite
    float temperature() { return _temp; } // °C
    float humidity() { return _hum; }     // %
private:
    static const int PIN_DATA = 23;
    static const unsigned long READ_INTERVAL_MS = 2000; // DHT11 lent
    float _temp = 0;
    float _hum = 0;
    unsigned long _lastRead = 0;
};
