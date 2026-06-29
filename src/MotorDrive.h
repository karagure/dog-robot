#pragma once
#include <Arduino.h>

class MotorDrive {
public:
    void begin();
    void setLeftRight(int leftPct, int rightPct);
    void stop();
private:
    // DRV8833 #1 (gauche) : moteurs A/B ; #2 (droite) : moteurs A/B
    static const int L_AIN1 = 32, L_AIN2 = 33, L_BIN1 = 25, L_BIN2 = 4;
    static const int R_AIN1 = 16, R_AIN2 = 17, R_BIN1 = 18, R_BIN2 = 19;
    void driveMotor(int in1Ch, int in2Ch, int pct);
};
