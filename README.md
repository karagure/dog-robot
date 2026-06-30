# Chien robot 🐕

Firmware ESP32 pour un chien robot : 4 servos d'épaules (3 poses) + 4 moteurs DC
(déplacement différentiel), piloté depuis une page web en Bluetooth (BLE).

## Matériel

- **ESP32** DevKit v1 (`esp32doit-devkit-v1`)
- **4x servo MG996R** (épaules) — alim **4,8–7,2 V** (5–6 V conseillé). ⚠️ **3,3 V insuffisant**, le servo ne bouge pas.
- **4x moteur DC** pilotés par **2x DRV8833**
- **1x DHT11** (température + humidité)
- Batterie moteurs séparée de l'alim servos.

## Câblage

### Servos (signal sur l'ESP32)

| Servo            | GPIO |
|------------------|------|
| Avant gauche     | 13   |
| Avant droite     | 14   |
| Arrière gauche   | 27   |
| Arrière droite   | 26   |

### Moteurs DC — DRV8833 #1 (côté gauche)

| Entrée | GPIO |
|--------|------|
| AIN1   | 32   |
| AIN2   | 33   |
| BIN1   | 25   |
| BIN2   | 4    |

### Moteurs DC — DRV8833 #2 (côté droite)

| Entrée | GPIO |
|--------|------|
| AIN1   | 16   |
| AIN2   | 17   |
| BIN1   | 18   |
| BIN2   | 19   |

### Capteur DHT11

| Broche DHT11 | ESP32 |
|--------------|-------|
| Data         | GPIO 23 |
| VCC          | 3,3 V ou 5 V |
| GND          | GND |

> Le DHT11 envoie température + humidité à l'app toutes les ~2 s (affichées dans
> l'encart « Capteur »). L'encart passe en rouge au-delà de **45 °C** (seuil
> réglable via `TEMP_ALERT` dans `app/control.html`).

> ⚠️ **Alimentations** : servos en 5–6 V (fort courant), moteurs sur leur propre
> batterie. **Toutes les masses (GND) doivent être communes** avec l'ESP32.
> Ne pas alimenter les servos depuis le régulateur 3,3 V de la carte.

## Compiler et flasher

```bash
# Compiler
~/.platformio/penv/bin/pio run -e esp32doit-devkit-v1

# Flasher (ESP32 branché en USB)
~/.platformio/penv/bin/pio run -e esp32doit-devkit-v1 -t upload

# Moniteur série
~/.platformio/penv/bin/pio device monitor -b 115200
```

## Tests (logique pure)

```bash
~/.platformio/penv/bin/pio test -e native -f test_commands
```

## Utilisation (interface mobile)

1. Allumer le robot. L'ESP32 s'annonce en BLE sous le nom **`ChienRobot`**.
2. Sur **Android**, ouvrir `app/control.html` dans **Chrome** (Web Bluetooth
   nécessite Chrome/Edge ; non supporté par Safari/iOS).
3. Appuyer sur **Connecter** et choisir `ChienRobot`.
4. Boutons : **Penché avant**, **Grand écart**, **Équilibre**. Le **joystick**
   fait avancer/reculer/tourner. **STOP** coupe les moteurs.

> Sécurité : si aucune commande de déplacement n'arrive pendant 500 ms, ou si la
> connexion BLE tombe, les moteurs s'arrêtent automatiquement.

## Protocole BLE

Service `0000ffe0-…`, caractéristique d'écriture `0000ffe1-…`. Commandes texte
terminées par `\n` :

| Commande     | Effet                                   |
|--------------|-----------------------------------------|
| `D,<x>,<y>`  | Déplacement, `x`,`y` ∈ [-100,100]       |
| `P1`         | Pose penché avant                       |
| `P2`         | Pose grand écart                        |
| `P3`         | Pose équilibre                          |
| `S`          | Stop                                    |

## Ajuster les poses

Les angles des 3 poses sont en haut de `src/ShoulderPose.cpp`, tableau `POSES`
(ordre : avant-G, avant-D, arrière-G, arrière-D). Modifie ces valeurs selon ton
montage mécanique, puis recompile/reflashe. La vitesse de mouvement se règle via
`DEG_PER_SEC` dans le même fichier.
