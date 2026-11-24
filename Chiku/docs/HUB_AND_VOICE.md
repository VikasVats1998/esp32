# Hub, sensors, wake word

## Hub server (client / server model)
`python3 Server/server.py` on a PC (stdlib only). The robot is the client:
- It finds the server by itself: the server broadcasts `{"hub":"chiku","port":8080}` on UDP 41234 every 2 s and the robot
  stores the address. A hand-set address (`POST /api/set {"serverUrl":"http://pc:8080"}`) is kept unless the server is
  unreachable for 30 s.
- Once a second the robot sends telemetry (tilt, roll/pitch, gyro, sound level, battery, USB, core temperature, Wi-Fi,
  free memory) and device changes, and gets the full device list back (lamp, fan, switch, climate ...).
- Web dashboard `http://<pc>:8080/`, phone app tab **Hub** (RobotApp). Both show live sensors, history charts, devices
  and let you change clap wake / sensitivity / wake word / recalibrate.
- Token: default `chiku` (`--token` on the server, `serverToken` on the robot).
- AI voice: `POST /api/ai/chat` is a stub (`ai_hook()` in server.py) for the assistant to come.

## Screens (UI kit style)
Hub screens are in the swipe carousel next to the face: Face, Clock, Weather, **Home** (clock + device tiles),
**Light** (dimmer ring), **Fan** (speed ring), Timer. Swipe up for the menu ("Home" ring opens the device tiles).
**Motion** (bubble level, gyro, recalibrate): Sensors screen, IMU tile. **System** (health, clap wake, wake word,
speaker, reboot, sync, power off): Settings, "Hub & voice". Navigation of every screen: [README.md](README.md).

## Sensors
- IMU (QMI8658): gyro zero learned at boot (keep the robot still for the first second), fused roll/pitch, shake, pick-up,
  tap, fall, upside-down. Recalibrate: Motion screen button, dashboard, or `POST /api/imu/calibrate`.
- Microphones (ES7210): loudness meter, single / double clap. Setting `clapWake`, `clapSens` (0-100).
- Power (AXP2101): battery %, volts, charging, USB volts; core temperature from the ESP32-S3.

## Wake word and voice commands (ESP-SR, on the board)
Say **"Hi ESP"**, the robot looks up and beeps, then say one command within ~6 s:
turn on/off the light, start/stop fan, brighter, dimmer, go to sleep, be happy, dance.
Light and fan act on the first light / fan of the hub server. Phonemes live in `src/head/wake.cpp`
(generate more with `ESP_SR/tools/gen_sr_commands.py`).

One-time setup per board: `platformio.ini` uses the `esp_sr_16.csv` partitions (3 MB app x2, 7 MB LittleFS, 'model'),
so after the first upload run `pio run -d Chiku -e head -t uploadfs` and `Chiku/tools/flash_models.sh`.
The engine starts ~9 s after boot (after Wi-Fi, because both need internal RAM). `Settings.wakeWord` turns it off.

## Personality (face screen)
The UI kit rules win where they overlap (see README, "Using Chiku"):
tap = happy / wink / love, 5 fast taps: angry, 9: rage. Hold the screen 3 s = power menu.
Shake = angry, shaken again within 6 s = rage. Small tilt: eyes look downhill; big tilt = dizzy.
Upside down = scared. Pick up = surprised. Loud sound = surprised. Clap: wakes it (surprised), double clap: excited.
"Hi ESP" = listening face, then the voice command. Battery < 15 %: tired yellow "feed me!" face, plugged in:
green charging ring for a few seconds.

## Memory notes
LVGL's heap is in PSRAM (`src/head/lv_mem_psram.c`), and so is the full screen frame used for the free screen
rotation; internal RAM is kept for Wi-Fi, the DMA strips and the speech engine.
