# Chien robot — Firmware ESP32 + interface mobile BLE

Date : 2026-06-29
Plateforme : ESP32 (esp32doit-devkit-v1), framework Arduino, PlatformIO

## Objectif

Firmware pour piloter un chien robot :
- **4 servos MG996R** (épaules) avec 3 poses nommées.
- **4 moteurs DC** via **2x DRV8833** pour avancer / reculer / tourner (commande différentielle gauche/droite).
- **Interface mobile Bluetooth (BLE)** : page web (Web Bluetooth, Chrome Android) avec boutons de pose, joystick de déplacement et STOP.

## Architecture

Firmware modulaire, composants isolés à interface claire :

- **`MotorDrive`** — pilote les 2x DRV8833. API : `setLeftRight(int leftPct, int rightPct)` avec valeurs [-100,100] (négatif = arrière), `stop()`. Convertit en PWM + sens sur les paires de broches.
- **`ShoulderPose`** — gère les 4 servos MG996R et les 3 poses. API : `begin()`, `goToPose(Pose p)`, `update()` (interpolation douce non-bloquante vers les angles cibles). Tableau d'angles configurable en tête de fichier.
- **`BleControl`** — serveur BLE GATT. Reçoit des commandes texte (caractéristique write), callback vers le dispatcher. Notify optionnel pour l'état.
- **`main.cpp`** — init, dispatch des commandes, boucle non-bloquante appelant `ShoulderPose::update()`.

Côté téléphone : **une page HTML unique** (`app/control.html`) en Web Bluetooth.

## Protocole de commandes BLE

Service BLE unique, caractéristique en écriture. Commandes texte terminées par `\n` :

| Commande | Effet |
|----------|-------|
| `D,<x>,<y>` | Déplacement. `x`,`y` ∈ [-100,100] (joystick). Mixage différentiel. |
| `P1` | Pose **Penché avant** |
| `P2` | Pose **Grand écart** |
| `P3` | Pose **Équilibre** (roues rapprochées sous le robot) |
| `S` | STOP (arrêt moteurs DC) |

Caractéristique notify : renvoie la pose courante / état (optionnel, pour affichage app).

UUIDs (custom) :
- Service : `0000ffe0-0000-1000-8000-00805f9b34fb`
- Caractéristique commande (write) : `0000ffe1-0000-1000-8000-00805f9b34fb`
- Caractéristique état (notify) : `0000ffe2-0000-1000-8000-00805f9b34fb`

## Mapping GPIO (ESP32 DevKit v1)

Broches PWM-capables ; évite pins de boot (0,2,12,15) et input-only (34-39).

- **Servos** : `GPIO 13, 14, 27, 26` → avant-G, avant-D, arrière-G, arrière-D
- **DRV8833 #1 (gauche)** : AIN1/AIN2 = `32/33`, BIN1/BIN2 = `25/4`
- **DRV8833 #2 (droite)** : AIN1/AIN2 = `16/17`, BIN1/BIN2 = `18/19`

Alimentations séparées : servos 5–6 V (fort courant), moteurs sur batterie, **masses communes** avec l'ESP32.

Gestion canaux LEDC : ESP32Servo alloue ses propres timers pour les 4 servos ; les 8 sorties PWM des DRV8833 utilisent des canaux LEDC distincts via `ledcSetup`/`ledcAttachPin`. Vérifier l'absence de conflit de canal/timer à l'init.

## Les 3 poses (angles de départ, à ajuster mécaniquement)

| Pose | avG | avD | arG | arD |
|------|-----|-----|-----|-----|
| P1 Penché avant | 120° | 60° | 100° | 80° |
| P2 Grand écart | 150° | 30° | 150° | 30° |
| P3 Équilibre | 90° | 90° | 90° | 90° |

Valeurs symétriques de départ, affinables. Interpolation : vitesse limitée (ex. ~120°/s) pour mouvement doux.

## Joystick → moteurs (mixage différentiel)

```
gauche = clamp(y + x, -100, 100)
droite = clamp(y - x, -100, 100)
```
`y` positif = avancer ; `x` = rotation. Chaque côté → vitesse PWM + sens sur le DRV8833 correspondant.

## DRV8833 — pilotage d'un moteur

Pour chaque moteur (paire de broches IN1/IN2) :
- Avant : IN1 = PWM(vitesse), IN2 = 0
- Arrière : IN1 = 0, IN2 = PWM(vitesse)
- Stop (roue libre) : IN1 = IN2 = 0

Les 2 moteurs d'un même côté reçoivent la même commande.

## Sécurité / robustesse

- À l'init : moteurs arrêtés, servos en pose Équilibre.
- Watchdog déplacement : si aucune commande `D` reçue depuis ~500 ms, arrêt automatique des moteurs (sécurité perte de connexion BLE).
- À la déconnexion BLE : arrêt moteurs.

## Tests / validation

- Test unitaire du mixage différentiel (`leftRight` à partir de x,y) — logique pure, testable hors cible.
- Test parsing des commandes (chaîne → action).
- Validation matérielle manuelle : chaque pose, chaque direction, le watchdog.

## Structure des fichiers

```
src/
  main.cpp
  MotorDrive.h / .cpp
  ShoulderPose.h / .cpp
  BleControl.h / .cpp
  Commands.h          (parsing + mixage, logique pure testable)
app/
  control.html        (Web Bluetooth)
test/
  test_commands/      (tests logique pure)
platformio.ini        (+ lib_deps: ESP32Servo)
```
