# ESP32 robots and accessories

| Folder | What | Board | Status |
|---|---|---|---|
| [Chiku](Chiku/docs/README.md) | robot head: face, feelings, servos, sonar, voice, clock, weather, Wi-Fi app API, + ESP32-CAM | Waveshare ESP32-S3-Touch-AMOLED-1.75 | builds, not yet tested on hardware |
| [Jinu](Jinu/docs/README.md) | second robot: face, feelings, 5 touch pads, servos, sonar, voice | Waveshare ESP32-C6-LCD-1.47 | builds, not yet tested on hardware |
| [Desk](Desk/docs/README.md) | standing-desk accessory: presses the handset buttons, reads the height, secure link | ESP32-S3 Super Mini | builds, wiring unverified |
| [RobotApp](RobotApp/README.md) | phone app (Expo SDK 58) to control and test everything | iOS / Android | builds, tested against mocks |
| [Bringup](Bringup/README.md) | laptop toolkit: flash, test every board, write a report to paste | Python | tested against simulators |
| [Probe](Probe/README.md) | read-only measuring firmware (voltages, edges, serial) for unknown electronics | ESP32-S3 Super Mini | builds |
| [Shared](Shared) | `DeskLink` (secure desk link) and `RemoteLink` (radio frames) libraries | - | PC tests pass |
| [Gimbal](Gimbal/docs/README.md), [Remote](Remote/docs/README.md) | idea: radio remote for a DJI Osmo Mobile SE | ESP32-S3 Super Mini | **paused** (only an inquiry) |
| [Emo](Emo/README.md) | older ESP32-C6 EMO Arduino sketches | ESP32-C6-LCD-1.47 | old |

## Build

```bash
pip install platformio "click<8.2"          # click 8.2+ breaks the bundled esptool
pio run -d Chiku -e head                     # also: -e cam
pio run -d Jinu -e jinu
pio run -d Desk -e desk
cd RobotApp && npm install && npx expo start
```

## Test

| Command | Checks |
|---|---|
| `Shared/DeskLink/test/run.sh` | desk crypto: C++ firmware vs TypeScript app |
| `Shared/RemoteLink/test/run.sh` | radio frames, replay, pairing |
| `cd RobotApp && npx tsx tools/smoke.mts localhost:8787 chiku` | app API client against `node tools/mock-robot.mjs` |
| `cd RobotApp && npx tsx tools/smoke-desk.mts localhost:8790` | desk client against `npx tsx tools/mock-desk.mts` |
| `python3 Bringup/bringup.py ...` | on the laptop with real boards, see [Bringup/README.md](Bringup/README.md) |

Each project has its own `docs/` (wiring, testing, security where relevant).
