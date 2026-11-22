# ServoTest: 4-5 servos on a PCA9685 with a plain ESP32

Serial-monitor test for servos on a 16-channel PCA9685 driver. Works with any normal ESP32 dev board with USB-C.
The servos do **not** move at power-on; you start each test from the keyboard.

## Wiring

| PCA9685 | ESP32 |
|---|---|
| VCC | 3V3 |
| GND | GND |
| SDA | GPIO21 |
| SCL | GPIO22 |
| V+ (green screw terminal) | **separate 5-6 V supply (+)** |
| GND of that supply | joined to ESP32 GND |

Servos plug into channels **0-4** (brown/black = GND side, red = middle, orange/yellow = signal on the PWM pin side).
**Never power the servos from the ESP32 3V3 or USB 5V pin**: 4-5 servos draw 1-3 A when they start or stall, which resets the ESP32.
Use a supply of at least 2 A. Metal-gear servos (MG90S) draw more than plastic ones (SG90).

## Run

```bash
pip install platformio "click<8.2"
cd ServoTest
pio run -e servotest -t upload
pio device monitor -b 115200
```

## Commands (type, then Enter)

| Command | Does |
|---|---|
| `h` | help |
| `s` | I2C scan: the PCA9685 must appear at `0x40` |
| `c` | centre all servos (90 deg), one by one |
| `a 2 45` | move servo on channel 2 to 45 deg |
| `p 0 1500` | raw pulse (500-2400 us) on channel 0 |
| `t` | test: each servo in turn 90, 45, 135, 90 |
| `w` | sweep all servos together, 3 times |
| `o` | all outputs off (servos go limp) |

Change `SERVO_COUNT` (4 or 5), `US_MIN` / `US_MAX`, `SPEED_DPS` at the top of `src/main.cpp`.

## If something does not work

* `PCA9685 NOT FOUND`: check SDA/SCL (GPIO21/22), 3V3 on VCC, common GND; run `s`.
* Found, but servos do not move: V+ terminal needs the separate 5-6 V supply (VCC alone only powers the chip).
* Servo buzzes at an end: its range is smaller than 500-2400 us; use `p` to find the limits and edit `US_MIN` / `US_MAX`.
* ESP32 resets when servos move: the supply is too weak or shares the USB power; use a bigger supply.
