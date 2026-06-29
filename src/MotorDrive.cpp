#include "MotorDrive.h"

// Canaux LEDC dédiés aux 8 entrées DRV8833 (8..15) pour éviter tout conflit
// avec ESP32Servo qui utilise les canaux/timers bas.
static const int CH_L_AIN1 = 8,  CH_L_AIN2 = 9,  CH_L_BIN1 = 10, CH_L_BIN2 = 11;
static const int CH_R_AIN1 = 12, CH_R_AIN2 = 13, CH_R_BIN1 = 14, CH_R_BIN2 = 15;
static const int PWM_FREQ = 1000, PWM_RES = 8; // 8 bits -> 0..255

void MotorDrive::begin() {
    const int pins[8] = {L_AIN1, L_AIN2, L_BIN1, L_BIN2, R_AIN1, R_AIN2, R_BIN1, R_BIN2};
    const int chans[8] = {CH_L_AIN1, CH_L_AIN2, CH_L_BIN1, CH_L_BIN2,
                          CH_R_AIN1, CH_R_AIN2, CH_R_BIN1, CH_R_BIN2};
    for (int i = 0; i < 8; i++) {
        ledcSetup(chans[i], PWM_FREQ, PWM_RES);
        ledcAttachPin(pins[i], chans[i]);
        ledcWrite(chans[i], 0);
    }
    stop();
}

void MotorDrive::driveMotor(int in1Ch, int in2Ch, int pct) {
    int duty = map(abs(pct), 0, 100, 0, 255);
    if (pct >= 0) { ledcWrite(in1Ch, duty); ledcWrite(in2Ch, 0); }
    else          { ledcWrite(in1Ch, 0);    ledcWrite(in2Ch, duty); }
}

void MotorDrive::setLeftRight(int leftPct, int rightPct) {
    driveMotor(CH_L_AIN1, CH_L_AIN2, leftPct);
    driveMotor(CH_L_BIN1, CH_L_BIN2, leftPct);
    driveMotor(CH_R_AIN1, CH_R_AIN2, rightPct);
    driveMotor(CH_R_BIN1, CH_R_BIN2, rightPct);
}

void MotorDrive::stop() {
    setLeftRight(0, 0);
}
