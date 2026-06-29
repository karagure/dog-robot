#include <Arduino.h>
#include "Commands.h"
#include "MotorDrive.h"
#include "ShoulderPose.h"
#include "BleControl.h"

static MotorDrive motors;
static ShoulderPose shoulders;
static BleControl ble;

static unsigned long lastDrive = 0;
static const unsigned long DRIVE_TIMEOUT_MS = 500;

static void handleLine(const char* line) {
    Command c = parseCommand(line);
    switch (c.type) {
        case CmdType::Drive: {
            DriveOutput o = mixDifferential(c.x, c.y);
            motors.setLeftRight(o.left, o.right);
            lastDrive = millis();
            break;
        }
        case CmdType::Pose: {
            shoulders.goToPose(c.pose);
            char buf[8];
            snprintf(buf, sizeof(buf), "P%d", c.pose);
            ble.notifyState(buf);
            break;
        }
        case CmdType::Stop:
            motors.stop();
            break;
        case CmdType::None:
        default:
            break;
    }
}

void setup() {
    Serial.begin(115200);
    motors.begin();
    shoulders.begin();
    ble.begin("ChienRobot", handleLine);
    lastDrive = millis();
}

void loop() {
    shoulders.update();
    unsigned long now = millis();
    if (now - lastDrive > DRIVE_TIMEOUT_MS) {
        motors.stop();
        lastDrive = now; // évite un stop répété inutile
    }
    if (!ble.isConnected()) {
        motors.stop();
    }
    delay(10);
}
