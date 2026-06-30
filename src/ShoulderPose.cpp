#include "ShoulderPose.h"

// Poses (avG, avD, arG, arD) — à affiner selon le montage mécanique réel.
static const int POSES[4][4] = {
    {120, 60, 100, 80},  // P1 penché avant
    {150, 30, 150, 30},  // P2 grand écart
    { 90, 90,  90, 90},  // P3 équilibre
    {  0,  0,   0,  0},  // P4 stop
};
static const float DEG_PER_SEC = 120.0f; // vitesse d'interpolation

void ShoulderPose::begin() {
    const int pins[4] = {PIN_AVG, PIN_AVD, PIN_ARG, PIN_ARD};
    for (int i = 0; i < 4; i++) {
        _servos[i].setPeriodHertz(50);
        _servos[i].attach(pins[i], 500, 2500);
        _current[i] = POSES[2][i];
        _target[i] = POSES[2][i];
        _servos[i].write((int)_current[i]);
    }
    _pose = 4;
    _lastUpdate = millis();
}

void ShoulderPose::goToPose(int pose) {
    if (pose < 1 || pose > 4) return;
    _pose = pose;
    for (int i = 0; i < 4; i++) _target[i] = POSES[pose - 1][i];
}

void ShoulderPose::update() {
    unsigned long now = millis();
    float dt = (now - _lastUpdate) / 1000.0f;
    _lastUpdate = now;
    float maxStep = DEG_PER_SEC * dt;
    for (int i = 0; i < 4; i++) {
        float diff = _target[i] - _current[i];
        if (fabs(diff) <= maxStep) _current[i] = _target[i];
        else _current[i] += (diff > 0 ? maxStep : -maxStep);
        _servos[i].write((int)_current[i]);
    }
}
