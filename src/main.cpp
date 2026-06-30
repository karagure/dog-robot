#include <Arduino.h>
#include "Commands.h"
#include "MotorDrive.h"
#include "ShoulderPose.h"
#include "BleControl.h"
#include "EnvSensor.h"

static MotorDrive motors;
static ShoulderPose shoulders;
static BleControl ble;
static EnvSensor env;

static unsigned long lastDrive = 0;
static const unsigned long DRIVE_TIMEOUT_MS = 500;

// LED témoin (LED bleue intégrée du DevKit v1) : s'allume brièvement à chaque
// commande reçue par Bluetooth, pour vérifier que la liaison fonctionne.
static const int LED_PIN = 2;
static unsigned long ledOffAt = 0;

static void handleLine(const char* line) {
    // Témoin : impulsion à chaque ligne reçue, valide ou non.
    digitalWrite(LED_PIN, HIGH);
    ledOffAt = millis() + 60;

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
    delay(200);
    Serial.println("\n[BOOT] demarrage...");
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    motors.begin();
    Serial.println("[BOOT] moteurs OK");
    shoulders.begin();
    Serial.println("[BOOT] servos OK");
    env.begin();
    Serial.println("[BOOT] DHT11 OK");
    ble.begin("ChienRobot", handleLine);
    Serial.println("[BOOT] BLE en emission sous 'ChienRobot' - PRET");
    lastDrive = millis();
}

void loop() {
    shoulders.update();
    unsigned long now = millis();

    // Éteint la LED témoin après l'impulsion.
    if (ledOffAt && now > ledOffAt) {
        digitalWrite(LED_PIN, LOW);
        ledOffAt = 0;
    }

    if (now - lastDrive > DRIVE_TIMEOUT_MS) {
        motors.stop();
        lastDrive = now; // évite un stop répété inutile
    }
    if (!ble.isConnected()) {
        motors.stop();
    }

    // Lecture capteur DHT11 et envoi vers l'app (température + humidité).
    if (env.update()) {
        char buf[24];
        snprintf(buf, sizeof(buf), "ENV,%.1f,%.0f", env.temperature(), env.humidity());
        ble.notifyState(buf);
    }

    delay(10);
}
