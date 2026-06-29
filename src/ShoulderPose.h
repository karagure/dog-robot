#pragma once
#include <Arduino.h>
#include <ESP32Servo.h>

class ShoulderPose {
public:
    void begin();
    void goToPose(int pose);
    void update();
    int currentPose() { return _pose; }
private:
    static const int PIN_AVG = 13, PIN_AVD = 14, PIN_ARG = 27, PIN_ARD = 26;
    Servo _servos[4];
    float _current[4];
    int _target[4];
    int _pose = 3;
    unsigned long _lastUpdate = 0;
};
