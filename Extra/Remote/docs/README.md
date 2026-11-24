# Gimbal remote (handheld)

ESP32-S3 Super Mini + analog joystick + 4 buttons + small LiPo. Sends the joystick and button state to the gimbal module (see [../../Gimbal/docs/README.md](../../Gimbal/docs/README.md)).

## Parts

| Qty | Part |
|---|---|
| 1 | ESP32-S3 Super Mini |
| 1 | 2-axis analog thumb joystick module with push button (KY-023 type) |
| 4 | tactile buttons (or arcade buttons) |
| 1 | LiPo 1S 400-1000 mAh with protection, TP4056 charger module, slide switch |
| 2 | 100 k ohm (battery voltage divider) |
| 1 | passive buzzer (optional) |

## Wiring

| Part | Super Mini |
|---|---|
| joystick VCC / GND | 3V3 / GND |
| joystick VRx / VRy | GPIO1 / GPIO2 |
| joystick SW | GPIO9 (to GND when pressed) |
| B1 capture | GPIO5 to GND |
| B2 zoom in | GPIO6 to GND |
| B3 zoom out | GPIO7 to GND |
| B4 mode | GPIO8 to GND |
| battery + (after the switch) | TP4056 OUT+ -> 5V pin; also 100 k -> GPIO4 -> 100 k -> GND (battery voltage divider) |
| buzzer (optional) | GPIO10 and GND |

Buttons use the internal pull-ups, no resistors needed. All five button pins can wake the remote from deep sleep. Pins are in `include/pins.h`.
Keep the stick **at rest while switching on**: the remote measures the stick centre at start-up.

## Build

```bash
pio run -d Extra/Remote -e remote -t upload
Extra/Remote/test/run.sh          # joystick mapping tests on the PC
```

Serial console (115200): `status`, `pair`, `unpair`, `calibrate`, `fine`, `factory`.
Controls and LED colours: [../../Gimbal/docs/USAGE.md](../../Gimbal/docs/USAGE.md).

## Tuning (`include/config.h`)

`JOY_DEADZONE`, `JOY_EXPO` (softness around the centre), `JOY_FINE_SCALE`, `JOY_INVERT_X/Y`, `JOY_SWAP_XY`, `JOY_SMOOTHING`, timings of taps and long press, sleep time.
