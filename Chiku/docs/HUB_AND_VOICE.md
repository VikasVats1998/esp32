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

## Screens (swipe left from the face)
Face, Clock, Weather, Timer, Sonar, Camera, Settings, **Light**, **Devices**, **Fan**, **Motion**, **System**.
Long press on the face = heart eyes; long press on any other screen = menu.

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
Tap: happy / surprised and looks at the finger. Double tap: wink. 5 fast taps: angry, 9: rage. Long press: love.
Shake: dizzy, then grumpy; shaken again while grumpy: rage. Pick up: surprised. Tilt: eyes look downhill.
Plug in the charger: excited; unplug: surprised; battery <= 15 %: sad (<= 5 %: sleepy).
Clap: wakes it, double clap: excited.

## Memory notes
LVGL's heap is in PSRAM (`src/head/lv_mem_psram.c`); internal RAM is kept for Wi-Fi and the speech engine.
