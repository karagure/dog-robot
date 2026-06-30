#include "EnvSensor.h"
#include "DHTesp.h"

static DHTesp dht;

void EnvSensor::begin() {
    dht.setup(PIN_DATA, DHTesp::DHT11);
    _lastRead = millis();
}

bool EnvSensor::update() {
    unsigned long now = millis();
    if (now - _lastRead < READ_INTERVAL_MS) return false;
    _lastRead = now;

    TempAndHumidity d = dht.getTempAndHumidity();
    if (dht.getStatus() != DHTesp::ERROR_NONE) return false;
    if (isnan(d.temperature) || isnan(d.humidity)) return false;

    _temp = d.temperature;
    _hum = d.humidity;
    return true;
}
