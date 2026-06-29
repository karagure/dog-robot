# Chien robot — Firmware ESP32 + interface mobile BLE — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Piloter un chien robot (4 servos MG996R d'épaules + 4 moteurs DC via 2x DRV8833) avec 3 poses et un déplacement différentiel, commandé depuis une page web BLE sur Android.

**Architecture:** Firmware Arduino/ESP32 modulaire. La logique pure (parsing de commandes + mixage différentiel) est isolée dans `Commands.h` et testée en natif (PlatformIO native env, TDD). Les modules matériels (`MotorDrive`, `ShoulderPose`, `BleControl`) encapsulent le hardware et sont validés manuellement. `main.cpp` orchestre. Une page HTML Web Bluetooth fournit l'UI.

**Tech Stack:** ESP32 (esp32doit-devkit-v1), Arduino framework, PlatformIO, ESP32Servo, BLE (NimBLE/Arduino BLE), Unity (tests natifs), HTML/JS Web Bluetooth.

## Global Constraints

- Plateforme cible : `esp32doit-devkit-v1`, framework `arduino`.
- Broches : servos `13,14,27,26` (avG,avD,arG,arD) ; DRV8833 #1 (gauche) `32/33`,`25/4` ; DRV8833 #2 (droite) `16/17`,`18/19`. Éviter pins de boot (0,2,12,15) et input-only (34-39).
- Valeurs joystick et vitesses : entiers bornés à `[-100, 100]`.
- Poses (avG,avD,arG,arD) : P1=`120,60,100,80` ; P2=`150,30,150,30` ; P3=`90,90,90,90`.
- Protocole BLE : commandes texte terminées par `\n` : `D,<x>,<y>`, `P1`, `P2`, `P3`, `S`.
- UUIDs : service `0000ffe0-0000-1000-8000-00805f9b34fb`, write `0000ffe1-...`, notify `0000ffe2-...`.
- Watchdog déplacement : arrêt moteurs si pas de `D` depuis 500 ms ; arrêt à la déconnexion BLE.
- À l'init : moteurs arrêtés, servos en pose Équilibre (P3).

---

### Task 1: Setup projet, dépendances et environnement de test natif

**Files:**
- Modify: `platformio.ini`
- Create: `.gitignore` (vérifier qu'il ignore `.pio/`)

**Interfaces:**
- Consumes: rien.
- Produces: deux environnements PlatformIO — `esp32doit-devkit-v1` (cible) et `native` (tests Unity hôte) ; dépendance `madhephaestus/ESP32Servo`.

- [ ] **Step 1: Réécrire `platformio.ini`**

```ini
[env:esp32doit-devkit-v1]
platform = espressif32
board = esp32doit-devkit-v1
framework = arduino
monitor_speed = 115200
lib_deps =
    madhephaestus/ESP32Servo@^3.0.5

[env:native]
platform = native
test_framework = unity
build_flags = -std=c++17
```

- [ ] **Step 2: Vérifier `.gitignore`**

S'assurer que `.pio/` y figure. Sinon, ajouter la ligne `.pio/`.

- [ ] **Step 3: Initialiser git si nécessaire et commit**

```bash
git init 2>/dev/null; git add platformio.ini .gitignore
git commit -m "chore: configure PlatformIO targets and ESP32Servo dependency"
```

---

### Task 2: Mixage différentiel (logique pure, TDD natif)

**Files:**
- Create: `src/Commands.h`
- Create: `test/test_commands/test_commands.cpp`

**Interfaces:**
- Consumes: rien.
- Produces:
  - `struct DriveOutput { int left; int right; };`
  - `int clampPct(int v);` → borne à [-100,100]
  - `DriveOutput mixDifferential(int x, int y);` → `left=clampPct(y+x)`, `right=clampPct(y-x)`

- [ ] **Step 1: Écrire le test qui échoue**

```cpp
#include <unity.h>
#include "Commands.h"

void test_clamp_bounds() {
    TEST_ASSERT_EQUAL_INT(100, clampPct(150));
    TEST_ASSERT_EQUAL_INT(-100, clampPct(-150));
    TEST_ASSERT_EQUAL_INT(42, clampPct(42));
}

void test_mix_forward() {
    DriveOutput o = mixDifferential(0, 80);
    TEST_ASSERT_EQUAL_INT(80, o.left);
    TEST_ASSERT_EQUAL_INT(80, o.right);
}

void test_mix_turn_right() {
    DriveOutput o = mixDifferential(50, 0);
    TEST_ASSERT_EQUAL_INT(50, o.left);
    TEST_ASSERT_EQUAL_INT(-50, o.right);
}

void test_mix_clamped() {
    DriveOutput o = mixDifferential(60, 80); // y+x=140 -> 100
    TEST_ASSERT_EQUAL_INT(100, o.left);
    TEST_ASSERT_EQUAL_INT(20, o.right);
}

void setUp() {} void tearDown() {}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_clamp_bounds);
    RUN_TEST(test_mix_forward);
    RUN_TEST(test_mix_turn_right);
    RUN_TEST(test_mix_clamped);
    return UNITY_END();
}
```

- [ ] **Step 2: Lancer le test, vérifier l'échec de compilation**

Run: `pio test -e native -f test_commands`
Expected: FAIL (`Commands.h` / fonctions introuvables).

- [ ] **Step 3: Implémenter le minimum dans `src/Commands.h`**

```cpp
#pragma once

struct DriveOutput { int left; int right; };

inline int clampPct(int v) {
    if (v > 100) return 100;
    if (v < -100) return -100;
    return v;
}

inline DriveOutput mixDifferential(int x, int y) {
    DriveOutput o;
    o.left = clampPct(y + x);
    o.right = clampPct(y - x);
    return o;
}
```

- [ ] **Step 4: Lancer le test, vérifier le succès**

Run: `pio test -e native -f test_commands`
Expected: PASS (4 tests).

- [ ] **Step 5: Commit**

```bash
git add src/Commands.h test/test_commands/test_commands.cpp
git commit -m "feat: differential mixing logic with tests"
```

---

### Task 3: Parsing des commandes (logique pure, TDD natif)

**Files:**
- Modify: `src/Commands.h`
- Modify: `test/test_commands/test_commands.cpp`

**Interfaces:**
- Consumes: `DriveOutput`, `mixDifferential` (Task 2).
- Produces:
  - `enum class CmdType { None, Drive, Pose, Stop };`
  - `struct Command { CmdType type; int x; int y; int pose; };` (`pose` ∈ {1,2,3} si `type==Pose`)
  - `Command parseCommand(const char* line);` — parse `D,<x>,<y>` / `P1`/`P2`/`P3` / `S`. Entrée invalide → `type=None`.

- [ ] **Step 1: Ajouter les tests qui échouent**

Ajouter dans `test_commands.cpp` (et les `RUN_TEST` dans `main`) :

```cpp
void test_parse_drive() {
    Command c = parseCommand("D,50,-30");
    TEST_ASSERT_EQUAL(CmdType::Drive, c.type);
    TEST_ASSERT_EQUAL_INT(50, c.x);
    TEST_ASSERT_EQUAL_INT(-30, c.y);
}

void test_parse_pose() {
    Command c = parseCommand("P2");
    TEST_ASSERT_EQUAL(CmdType::Pose, c.type);
    TEST_ASSERT_EQUAL_INT(2, c.pose);
}

void test_parse_stop() {
    Command c = parseCommand("S");
    TEST_ASSERT_EQUAL(CmdType::Stop, c.type);
}

void test_parse_invalid() {
    Command c = parseCommand("xyz");
    TEST_ASSERT_EQUAL(CmdType::None, c.type);
}
```

- [ ] **Step 2: Lancer, vérifier l'échec**

Run: `pio test -e native -f test_commands`
Expected: FAIL (`parseCommand` introuvable).

- [ ] **Step 3: Implémenter dans `src/Commands.h`**

```cpp
#include <cstdlib>
#include <cstring>

enum class CmdType { None, Drive, Pose, Stop };
struct Command { CmdType type; int x; int y; int pose; };

inline Command parseCommand(const char* line) {
    Command c{CmdType::None, 0, 0, 0};
    if (line == nullptr || line[0] == '\0') return c;
    if (line[0] == 'S') { c.type = CmdType::Stop; return c; }
    if (line[0] == 'P' && line[1] >= '1' && line[1] <= '3') {
        c.type = CmdType::Pose; c.pose = line[1] - '0'; return c;
    }
    if (line[0] == 'D' && line[1] == ',') {
        const char* p = line + 2;
        char* end = nullptr;
        long x = strtol(p, &end, 10);
        if (end == p || *end != ',') return c;
        p = end + 1;
        long y = strtol(p, &end, 10);
        if (end == p) return c;
        c.type = CmdType::Drive; c.x = (int)x; c.y = (int)y; return c;
    }
    return c;
}
```

- [ ] **Step 4: Lancer, vérifier le succès**

Run: `pio test -e native -f test_commands`
Expected: PASS (8 tests).

- [ ] **Step 5: Commit**

```bash
git add src/Commands.h test/test_commands/test_commands.cpp
git commit -m "feat: BLE command parsing with tests"
```

---

### Task 4: Module MotorDrive (2x DRV8833)

**Files:**
- Create: `src/MotorDrive.h`
- Create: `src/MotorDrive.cpp`

**Interfaces:**
- Consumes: rien.
- Produces:
  - `class MotorDrive { public: void begin(); void setLeftRight(int leftPct, int rightPct); void stop(); };`
  - `setLeftRight` : valeurs [-100,100] ; positif = avant, négatif = arrière. Les 2 moteurs d'un côté reçoivent la même commande.

- [ ] **Step 1: Écrire `src/MotorDrive.h`**

```cpp
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
```

- [ ] **Step 2: Écrire `src/MotorDrive.cpp`**

Utilise l'API LEDC (canaux 8..15 pour éviter les canaux bas pris par ESP32Servo). Chaque broche IN a son canal PWM.

```cpp
#include "MotorDrive.h"

// Canaux LEDC dédiés aux 8 entrées DRV8833 (8..15)
static const int CH_L_AIN1 = 8,  CH_L_AIN2 = 9,  CH_L_BIN1 = 10, CH_L_BIN2 = 11;
static const int CH_R_AIN1 = 12, CH_R_AIN2 = 13, CH_R_BIN1 = 14, CH_R_BIN2 = 15;
static const int PWM_FREQ = 1000, PWM_RES = 8; // 8 bits -> 0..255

void MotorDrive::begin() {
    const int pins[8] = {L_AIN1,L_AIN2,L_BIN1,L_BIN2,R_AIN1,R_AIN2,R_BIN1,R_BIN2};
    const int chans[8] = {CH_L_AIN1,CH_L_AIN2,CH_L_BIN1,CH_L_BIN2,CH_R_AIN1,CH_R_AIN2,CH_R_BIN1,CH_R_BIN2};
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
```

- [ ] **Step 3: Vérifier la compilation cible**

Run: `pio run -e esp32doit-devkit-v1`
Expected: SUCCESS (build sans erreur ; `main.cpp` actuel encore en place).

- [ ] **Step 4: Commit**

```bash
git add src/MotorDrive.h src/MotorDrive.cpp
git commit -m "feat: MotorDrive module for dual DRV8833 differential drive"
```

---

### Task 5: Module ShoulderPose (4 servos MG996R, interpolation douce)

**Files:**
- Create: `src/ShoulderPose.h`
- Create: `src/ShoulderPose.cpp`

**Interfaces:**
- Consumes: rien.
- Produces:
  - `class ShoulderPose { public: void begin(); void goToPose(int pose); void update(); int currentPose(); };`
  - `goToPose(1|2|3)` fixe les angles cibles ; `update()` (à appeler en boucle) interpole vers la cible (~120°/s) ; `begin()` initialise en pose 3.

- [ ] **Step 1: Écrire `src/ShoulderPose.h`**

```cpp
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
```

- [ ] **Step 2: Écrire `src/ShoulderPose.cpp`**

```cpp
#include "ShoulderPose.h"

// Poses (avG, avD, arG, arD)
static const int POSES[3][4] = {
    {120, 60, 100, 80},  // P1 penché avant
    {150, 30, 150, 30},  // P2 grand écart
    { 90, 90,  90, 90},  // P3 équilibre
};
static const float DEG_PER_SEC = 120.0f;

void ShoulderPose::begin() {
    const int pins[4] = {PIN_AVG, PIN_AVD, PIN_ARG, PIN_ARD};
    for (int i = 0; i < 4; i++) {
        _servos[i].setPeriodHertz(50);
        _servos[i].attach(pins[i], 500, 2500);
        _current[i] = POSES[2][i];
        _target[i] = POSES[2][i];
        _servos[i].write((int)_current[i]);
    }
    _pose = 3;
    _lastUpdate = millis();
}

void ShoulderPose::goToPose(int pose) {
    if (pose < 1 || pose > 3) return;
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
```

- [ ] **Step 3: Vérifier la compilation cible**

Run: `pio run -e esp32doit-devkit-v1`
Expected: SUCCESS.

- [ ] **Step 4: Commit**

```bash
git add src/ShoulderPose.h src/ShoulderPose.cpp
git commit -m "feat: ShoulderPose module with smooth servo interpolation"
```

---

### Task 6: Module BleControl (serveur BLE GATT)

**Files:**
- Create: `src/BleControl.h`
- Create: `src/BleControl.cpp`

**Interfaces:**
- Consumes: rien (callback fourni par `main`).
- Produces:
  - `class BleControl { public: void begin(const char* name, void (*onLine)(const char*)); void notifyState(const char* msg); bool isConnected(); };`
  - À chaque write reçu, découpe sur `\n` et appelle `onLine` par ligne.

- [ ] **Step 1: Écrire `src/BleControl.h`**

```cpp
#pragma once
#include <Arduino.h>

class BleControl {
public:
    void begin(const char* name, void (*onLine)(const char*));
    void notifyState(const char* msg);
    bool isConnected();
};
```

- [ ] **Step 2: Écrire `src/BleControl.cpp`**

```cpp
#include "BleControl.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define SERVICE_UUID "0000ffe0-0000-1000-8000-00805f9b34fb"
#define CHAR_WRITE_UUID "0000ffe1-0000-1000-8000-00805f9b34fb"
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
        for (int i = 0; i <= v.length(); i++) {
            if (i == v.length() || v[i] == '\n') {
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
        CHAR_WRITE_UUID, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
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
```

- [ ] **Step 3: Vérifier la compilation cible**

Run: `pio run -e esp32doit-devkit-v1`
Expected: SUCCESS.

- [ ] **Step 4: Commit**

```bash
git add src/BleControl.h src/BleControl.cpp
git commit -m "feat: BleControl GATT server with line-based command callback"
```

---

### Task 7: Orchestration `main.cpp` (dispatch + watchdog)

**Files:**
- Modify: `src/main.cpp` (remplacer le contenu)

**Interfaces:**
- Consumes: `Commands.h` (`parseCommand`, `mixDifferential`), `MotorDrive`, `ShoulderPose`, `BleControl`.
- Produces: firmware complet.

- [ ] **Step 1: Remplacer `src/main.cpp`**

```cpp
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
        case CmdType::Pose:
            shoulders.goToPose(c.pose);
            { char buf[8]; snprintf(buf, sizeof(buf), "P%d", c.pose); ble.notifyState(buf); }
            break;
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
        lastDrive = now; // évite stop répété inutile
    }
    if (!ble.isConnected()) {
        motors.stop();
    }
    delay(10);
}
```

- [ ] **Step 2: Compiler le firmware complet**

Run: `pio run -e esp32doit-devkit-v1`
Expected: SUCCESS (link de tous les modules).

- [ ] **Step 3: Relancer les tests natifs (non-régression)**

Run: `pio test -e native -f test_commands`
Expected: PASS (8 tests).

- [ ] **Step 4: Commit**

```bash
git add src/main.cpp
git commit -m "feat: main orchestration with command dispatch and safety watchdog"
```

---

### Task 8: Interface mobile — page Web Bluetooth

**Files:**
- Create: `app/control.html`

**Interfaces:**
- Consumes: protocole BLE (UUIDs, commandes `D,x,y` / `P1..3` / `S`).
- Produces: page autonome ouverte dans Chrome Android.

- [ ] **Step 1: Écrire `app/control.html`**

```html
<!DOCTYPE html>
<html lang="fr">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no">
<title>Chien robot</title>
<style>
  body { font-family: sans-serif; text-align: center; margin: 0; padding: 16px; background:#111; color:#eee; }
  button { font-size: 18px; padding: 14px; margin: 6px; border-radius: 10px; border:none; }
  .pose { background:#2a6; color:#fff; width: 30%; }
  #connect { background:#27e; color:#fff; width: 90%; }
  #stop { background:#c33; color:#fff; width: 90%; }
  #pad { width: 260px; height: 260px; margin: 16px auto; background:#222; border-radius:50%; position:relative; touch-action:none; }
  #knob { width: 80px; height: 80px; background:#27e; border-radius:50%; position:absolute; left:90px; top:90px; }
  #status { margin: 8px; font-size: 14px; color:#9c9; }
</style>
</head>
<body>
  <h2>🐕 Chien robot</h2>
  <button id="connect">Connecter (Bluetooth)</button>
  <div id="status">Déconnecté</div>
  <div>
    <button class="pose" data-p="P1">Penché avant</button>
    <button class="pose" data-p="P2">Grand écart</button>
    <button class="pose" data-p="P3">Équilibre</button>
  </div>
  <div id="pad"><div id="knob"></div></div>
  <button id="stop">STOP</button>

<script>
const SERVICE = "0000ffe0-0000-1000-8000-00805f9b34fb";
const CHAR_WRITE = "0000ffe1-0000-1000-8000-00805f9b34fb";
let writeChar = null;

async function send(str) {
  if (!writeChar) return;
  try { await writeChar.writeValue(new TextEncoder().encode(str + "\n")); } catch(e) {}
}

document.getElementById("connect").onclick = async () => {
  try {
    const dev = await navigator.bluetooth.requestDevice({ filters:[{ services:[SERVICE] }] });
    const server = await dev.gatt.connect();
    const svc = await server.getPrimaryService(SERVICE);
    writeChar = await svc.getCharacteristic(CHAR_WRITE);
    document.getElementById("status").textContent = "Connecté à " + dev.name;
    dev.addEventListener("gattserverdisconnected", () => {
      writeChar = null; document.getElementById("status").textContent = "Déconnecté";
    });
  } catch(e) { document.getElementById("status").textContent = "Erreur: " + e; }
};

document.querySelectorAll(".pose").forEach(b =>
  b.onclick = () => send(b.dataset.p));
document.getElementById("stop").onclick = () => { send("S"); resetKnob(); };

// Joystick
const pad = document.getElementById("pad"), knob = document.getElementById("knob");
const R = 90; let active = false, lastSent = 0;
function resetKnob(){ knob.style.left="90px"; knob.style.top="90px"; }
function handle(e){
  const t = e.touches ? e.touches[0] : e;
  const r = pad.getBoundingClientRect();
  let dx = t.clientX - r.left - 130, dy = t.clientY - r.top - 130;
  const d = Math.hypot(dx, dy);
  if (d > R){ dx = dx*R/d; dy = dy*R/d; }
  knob.style.left = (90 + dx) + "px";
  knob.style.top  = (90 + dy) + "px";
  const x = Math.round(dx / R * 100);
  const y = Math.round(-dy / R * 100);
  const now = Date.now();
  if (now - lastSent > 100){ send("D," + x + "," + y); lastSent = now; }
}
pad.addEventListener("touchstart", e => { active = true; handle(e); e.preventDefault(); });
pad.addEventListener("touchmove",  e => { if (active) handle(e); e.preventDefault(); });
pad.addEventListener("touchend",   () => { active = false; resetKnob(); send("D,0,0"); });
pad.addEventListener("mousedown", e => { active = true; handle(e); });
window.addEventListener("mousemove", e => { if (active) handle(e); });
window.addEventListener("mouseup", () => { if(active){ active=false; resetKnob(); send("D,0,0"); } });
</script>
</body>
</html>
```

- [ ] **Step 2: Vérification manuelle**

Ouvrir `app/control.html` dans Chrome Android (servir via `https`/`file` selon contexte ; Web Bluetooth exige un contexte sécurisé ou un fichier local). Cliquer « Connecter », sélectionner `ChienRobot`, tester poses + joystick + STOP.

- [ ] **Step 3: Commit**

```bash
git add app/control.html
git commit -m "feat: Web Bluetooth mobile control page"
```

---

### Task 9: Documentation de câblage et d'utilisation

**Files:**
- Create: `README.md`

**Interfaces:**
- Consumes: tout le projet.
- Produces: doc de montage + flash + usage.

- [ ] **Step 1: Écrire `README.md`**

Contenu : tableau de câblage (servos 13/14/27/26 ; DRV8833 #1 32/33/25/4 ; #2 16/17/18/19), note alimentations séparées + masses communes, commande de flash `pio run -e esp32doit-devkit-v1 -t upload`, ouverture de `app/control.html` dans Chrome Android, protocole de commandes, et comment ajuster les angles des poses dans `ShoulderPose.cpp`.

- [ ] **Step 2: Commit**

```bash
git add README.md
git commit -m "docs: wiring, flashing and usage guide"
```

---

## Self-Review

**Spec coverage :**
- 4 servos + 3 poses → Task 5 ✓
- 4 moteurs DC / 2x DRV8833 / différentiel → Task 4 + mixage Task 2 ✓
- Interface mobile BLE → Task 6 (firmware) + Task 8 (page web) ✓
- Protocole de commandes → Task 3 (parsing) + Task 7 (dispatch) ✓
- Mapping GPIO → Tasks 4, 5 (constantes) + Task 9 (doc) ✓
- Watchdog + arrêt à la déconnexion → Task 7 ✓
- Init en pose Équilibre → Task 5 (`begin`) ✓
- Tests logique pure → Tasks 2, 3 ✓

**Placeholder scan :** aucun TODO/TBD ; tout le code est fourni.

**Type consistency :** `DriveOutput{left,right}`, `Command{type,x,y,pose}`, `parseCommand`, `mixDifferential`, `setLeftRight`, `goToPose`, `update`, `currentPose`, `begin(name,onLine)`, `notifyState`, `isConnected` — cohérents entre tasks consommatrices (Task 7) et productrices (Tasks 2–6).

**Note :** les canaux LEDC des moteurs (8..15) sont volontairement hauts pour éviter tout conflit avec ESP32Servo (canaux/timers bas). À revérifier au runtime si un servo ou un moteur ne répond pas.
