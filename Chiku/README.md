# Chiku

Desk robot with a round AMOLED face (Waveshare ESP32-S3-Touch-AMOLED-1.75), 5 servos, touch pads, voice,
microphones, ESP32-CAM, a hub server and a phone app. Everything Chiku needs is in this folder.

| Folder | What | Status |
|---|---|---|
| [Firmware](Firmware/docs/README.md) | robot head: UI-kit face (22 moods) and screens, any-angle screen rotation, feelings + poses, servos, sonar, voice + microphones, "Hi ESP" wake word, clock, weather, games, hub screens, Wi-Fi API, + ESP32-CAM firmware | builds; display driver tested on the board, rest not yet |
| [Server](Firmware/docs/HUB_AND_VOICE.md) | hub server (`python3 Chiku/Server/server.py`): web dashboard, devices, sensor history, found by the robot by itself | tested with a simulated robot |
| [RobotApp](RobotApp/README.md) | phone app (Expo SDK 58) to control and test Chiku (also Jinu and the desk) | builds, tested against mocks |
| [Bringup](Bringup/README.md) | laptop toolkit: flash, test every board, write a report to paste | tested against simulators |
| [Desk](Desk/docs/README.md) | standing-desk accessory Chiku controls: presses the handset buttons, reads the height, secure link | builds, wiring unverified |
| [ServoTest](ServoTest/README.md) | serial test for the servos on the PCA9685 (found Chiku's real servo pins) | builds |
| [Shared/DeskLink](Shared/DeskLink) | secure desk link library (firmware C++, also used by the app and the Extra projects) | PC tests pass |
| [Backups](Backups) | flash backups of Chiku's board | - |
| Reference/DeskBuddy | pointer to the DeskBuddy project the display driver came from | - |

## Build

```bash
pio run -d Chiku/Firmware -e head          # robot head (also: -e cam for the ESP32-CAM)
pio run -d Chiku/Desk -e desk
```

## Upload to the boards

Run these on the computer the board is plugged into (USB data cable). Add `--upload-port COM5` (Windows) or
`--upload-port /dev/ttyACM0` if several boards are connected.

| Board | Before uploading | Commands |
|---|---|---|
| **Chiku head** (ESP32-S3 AMOLED) | USB-C. If it does not connect: hold **BOOT**, tap **PWR/RESET**, release BOOT | `pio run -d Chiku/Firmware -e head -t upload`, then `pio run -d Chiku/Firmware -e head -t uploadfs` (web page + voice clips), once per board `bash Chiku/Firmware/tools/flash_models.sh` (wake word) |
| **Chiku ESP32-CAM** | USB-TTL adapter: 5V-5V, GND-GND, TX-U0R, RX-U0T, **IO0 to GND**, press the cam **RESET** | `pio run -d Chiku/Firmware -e cam -t upload`, then remove IO0-GND and press RESET |
| **Desk module** (S3 Super Mini) | hold **BOOT** while plugging in USB-C | `pio run -d Chiku/Desk -e desk -t upload` |

After uploading, press RESET (or unplug/replug). Watch the log: `pio device monitor -b 115200`.
Or let the toolkit do it and check the result: `python3 Chiku/Bringup/bringup.py flash chiku` (also `chiku-cam`, `desk`).

First start: Chiku shows the first-time setup and opens the setup Wi-Fi `Chiku-Setup`; join it with the phone.
Later firmware updates can go over Wi-Fi: `http://chiku.local/update`, or the app (Settings, Update firmware).

## Phone app and hub

```bash
cd Chiku/RobotApp && npm install && npx expo start   # scan the QR code with Expo Go (phone on the same Wi-Fi)
python3 Chiku/Server/server.py                       # hub server on a PC, dashboard on http://<pc>:8080
```

## Test

| Command | Checks |
|---|---|
| `bash Chiku/Firmware/tools/uisim/run.sh` | Chiku UI on the PC: every face and screen as PNG + rotation math check |
| `Chiku/Shared/DeskLink/test/run.sh` | desk crypto: C++ firmware vs TypeScript app |
| `cd Chiku/RobotApp && npx tsx tools/smoke.mts localhost:8787 chiku` | app API client against `node tools/mock-robot.mjs` |
| `cd Chiku/RobotApp && npx tsx tools/smoke-desk.mts localhost:8790` | desk client against `npx tsx tools/mock-desk.mts` |
| `cd Chiku/RobotApp && npx tsx tools/smoke-hub.mts localhost:8080` | hub client against `Chiku/Server/server.py` |
| `bash Chiku/Bringup/tests/selftest.sh` | the test toolkit against simulators |
| `python3 Chiku/Bringup/bringup.py ...` | on the laptop with real boards, see [Bringup/README.md](Bringup/README.md) |

Firmware docs: [README](Firmware/docs/README.md), [WIRING](Firmware/docs/WIRING.md), [CALIBRATION](Firmware/docs/CALIBRATION.md),
[TESTING](Firmware/docs/TESTING.md), [HUB_AND_VOICE](Firmware/docs/HUB_AND_VOICE.md).
