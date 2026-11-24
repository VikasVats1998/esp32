# Universal Remote – ESP32-C3 Super Mini

One joystick remote for every project (Otto biped, 4-motor car, anything else). It sends joystick + button data over **ESP-NOW** (no router, no pairing, ~50 packets/s). Any ESP32 running a receiver sketch picks it up.

## Remote wiring (transmitter)

| Module | Module pin | ESP32-C3 pin | Note |
|---|---|---|---|
| Joystick | VCC | **3V3** | Use 3.3V, not 5V – the ADC pin can't take more |
| Joystick | GND | GND | |
| Joystick | VRx | **GPIO0** | Analog (ADC1) |
| Joystick | VRy | **GPIO1** | Analog (ADC1) |
| Joystick | SW | **GPIO3** | INPUT_PULLUP, LOW when pressed |
| Button | leg 1 | **GPIO10** | INPUT_PULLUP |
| Button | leg 2 | GND | LOW when pressed |

### Pins to avoid
| GPIO | Reason |
|---|---|
| 2, 8, 9 | Boot pins (9 = BOOT button, 8 = onboard LED) |
| 5 | ADC2 – analog doesn't work with Wi-Fi on (fine as digital) |
| 20, 21 | UART RX/TX – keep free for serial debug |

Still free on the remote: GPIO4 (analog), 5, 6, 7 (e.g. for an OLED, buzzer or LEDs later).

## What the remote sends

| Field | Range | Source |
|---|---|---|
| `x` | -100 (left) … +100 (right) | VRx, centre auto-calibrated at boot (**don't touch the stick while powering on**) |
| `y` | -100 (back) … +100 (forward) | VRy |
| `joyBtn` | 0/1 | Joystick click (GPIO3) |
| `btn` | 0/1 | Separate button held (GPIO10) |
| `mode` | 0–3 | **Short press** of the separate button cycles the mode |
| `seq` | 0–255 | Packet counter |

Deadzone is 8%. If a direction is inverted, change `INVERT_X` / `INVERT_Y` at the top of `RemoteJoystick_TX.ino`.

## Receivers

Both receivers default to an ESP32-C3 Super Mini; change the pin defines at the top of each sketch for other boards. They stop everything if no packet arrives for 300 ms (failsafe).

### Otto biped – `Receiver_Otto` (4 servos)

| Signal | GPIO |
|---|---|
| Left hip | 4 |
| Right hip | 5 |
| Left foot | 6 |
| Right foot | 7 |
| Servo V+ | separate 5V ≥ 2A supply (not the board!) |
| Servo GND | shared with board GND |

Controls: Y = walk forward/back (faster when pushed further), X = steer, or spin on the spot when Y is centred. Joystick click = wiggle.
Modes: 0 walk · 1 moonwalk shuffle · 2 swing dance · 3 up-down.
Calibrate with `trim[]` / `reverse[]`.

### 4-gear-motor car – `Receiver_Car` (L298N / TB6612 / DRV8833)

Left motors in parallel on channel A, right motors in parallel on channel B.

| Signal | GPIO | Driver pin |
|---|---|---|
| Left forward | 4 | IN1 |
| Left reverse | 5 | IN2 |
| Right forward | 6 | IN3 |
| Right reverse | 7 | IN4 |
| Horn/light (optional) | `PIN_AUX` (default off) | – |

- PWM is on the IN pins, so leave the L298N **ENA/ENB jumpers on** (always enabled).
- Motor supply to the driver's 12V/VM pin; **share GND** with the ESP32.
- Controls: Y = throttle, X = steering (tank mix). Mode = top speed 25/50/75/100 %. Joystick click = `PIN_AUX`.
- If a side spins backward, set `FLIP_LEFT` / `FLIP_RIGHT`.

### New project – `Receiver_Template`

Copy it, keep `onRecv`, `remoteBegin()` and `remoteAlive()`, and read `remote.x / .y / .joyBtn / .btn / .mode` anywhere.

## Build / flash

- Core: esp32 3.x (tested 3.3.12). Board `ESP32C3 Dev Module`, **USB CDC On Boot: Enabled**.
- Flash `RemoteJoystick_TX` to the remote, a receiver sketch to the robot/car. No MAC addresses to configure.
- ESP-NOW channel is `REMOTE_CHANNEL` (1) in `RemoteProtocol.h`. Every sketch folder holds an identical copy of that file – edit them all together.
- Several robots listening at once all react to the same remote (it broadcasts).
- Receiver on a different ESP32 (e.g. the C6 EMO board): works unchanged, just set the pins.

## Folder

```
Extra/RemoteJoystick/
  PINS.md
  RemoteProtocol.h          (master copy)
  RemoteJoystick_TX/        remote
  Receiver_Otto/            Otto biped
  Receiver_Car/             4-motor car
  Receiver_Template/        starting point for new projects
```
