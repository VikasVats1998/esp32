# Extra projects

Everything that is not Chiku. Some of them use Chiku's tools (phone app, test toolkit, DeskLink library in `Chiku/`).

| Folder | What | Board | Status |
|---|---|---|---|
| [Jinu](Jinu/docs/README.md) | second robot: face, feelings, 5 touch pads, servos, sonar, voice (phone app: `Chiku/RobotApp`) | Waveshare ESP32-C6-LCD-1.47 | builds, not yet tested on hardware |
| [OTTO](OTTO/PINS.md) | Otto biped with 4 servos: pins, calibration and servo tests | Waveshare ESP32-C6-LCD-1.47 | sketches |
| [RemoteJoystick](RemoteJoystick/PINS.md) | universal ESP-NOW joystick remote + receivers (Otto, car, template) | ESP32-C3 Super Mini | sketches |
| [Gimbal](Gimbal/docs/README.md), [Remote](Remote/docs/README.md) | idea: radio remote for a DJI Osmo Mobile SE | ESP32-S3 Super Mini | **paused** (only an inquiry) |
| [Shared/RemoteLink](Shared/RemoteLink) | radio frames library for Gimbal / Remote | - | PC tests pass |
| [Probe](Probe/README.md) | read-only measuring firmware (voltages, edges, serial) for unknown electronics | ESP32-S3 Super Mini | builds |
| [Emo](Emo/README.md) | older ESP32-C6 EMO Arduino sketches | ESP32-C6-LCD-1.47 | old |
| Reference/Lumo, Reference/pixel_plus | reference projects kept as plain files | - | - |

## Build and upload

```bash
pio run -d Extra/Jinu -e jinu -t upload && pio run -d Extra/Jinu -e jinu -t uploadfs   # Jinu: hold BOOT, tap RESET if it does not connect
pio run -d Extra/Probe -e probe -t upload                                               # Probe: hold BOOT while plugging in
python3 Chiku/Bringup/bringup.py flash jinu                                             # or with the test toolkit (also probe, gimbal, remote)
Extra/Shared/RemoteLink/test/run.sh                                                     # radio frame tests on the PC
```
First start: Jinu opens the setup Wi-Fi `Jinu-Setup`; later updates: `http://jinu.local/update`.
