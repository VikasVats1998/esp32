# Gimbal remote (DJI Osmo Mobile SE): module + handheld remote

Two small ESP32-S3 Super Mini projects that give the gimbal a **radio remote**:

```
 REMOTE (handheld)                                   GIMBAL MODULE (inside the Osmo handle)
 joystick, 4 buttons  --ESP-NOW, authenticated-->    PWM + RC filter  --> joystick wiper nodes
 ESP32-S3 Super Mini  <--status--                    photorelays      --> shutter / M / trigger / zoom button contacts
                                                     ESP32-S3 Super Mini
```

DJI publishes no protocol for the gimbal, so the module does not talk to the gimbal: it **imitates your fingers on its own controls** (stick voltage and button presses).
The gimbal and the DJI Mimo app cannot tell the difference, so everything you can do with the physical controls works from the remote.

Docs: [WIRING.md](WIRING.md) (read first) · [USAGE.md](USAGE.md) · [SECURITY.md](SECURITY.md) · [TESTING.md](TESTING.md) · remote hardware: [../../Remote/docs/README.md](../../Remote/docs/README.md)

## What works

| Function | How |
|---|---|
| Pan / tilt | remote joystick -> stick voltage on the gimbal |
| Capture (photo / record) | remote B1 held = gimbal shutter held |
| Zoom in / out | remote B2 / B3 (zoom buttons, or the zoom slider as an analog axis) |
| Photo / video, switch camera, ActiveTrack, lock, sport | macros (button patterns) from B4 and the joystick click |
| Recenter, flip / selfie | macros (trigger patterns) from the joystick click |
| Fine mode | joystick long press: all movements 45 %. |
| Safety | link lost > 250 ms: keys released, stick centred, then the gimbal's own joystick works again |

**What cannot be done by imitating buttons:** anything that only exists on the phone screen (for example *choosing* the subject for ActiveTrack by tapping it, menus, gallery).
If the gimbal has a physical gesture for it, a macro can press it; otherwise you must tap the phone.

## Build and flash

```bash
pip install platformio "click<8.2"
cd Gimbal && pio run -e gimbal -t upload      # in the gimbal handle
cd Remote && pio run -e remote -t upload      # the handheld remote
```

## First setup

1. Flash both. On the gimbal module open a serial terminal (115200): type `help`.
2. Bench test everything **before** soldering into the gimbal (TESTING.md).
3. Pair: power-cycle the gimbal module (2 minute pairing window, LED purple), then hold **B1 + B4** on the remote for 3 s.
4. Teach the module the gimbal's joystick voltages: `learn start`, move the physical stick to every extreme, `learn stop` (needs the optional ADC wires, see WIRING.md).
5. Check each macro against your manual and fix them with `macro set` (USAGE.md).

## Code map

| Folder | Content |
|---|---|
| `Shared/RemoteLink` | frame format, HMAC, replay protection, pairing (tested on a PC: `test/run.sh`), ESP-NOW wrapper |
| `Gimbal/src` | `outputs` (PWM sticks, key relays), `macros` (button sequences, editable live), `rlink` (receive, verify, failsafe, status), `console` (serial test/setup) |
| `Remote/src` | `inputs` (joystick shaping, gestures, calibration), `rlink` (send, pairing), `ui` (LED, buzzer, battery), `mapping` (tested on a PC) |
