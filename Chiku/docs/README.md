# Chiku firmware

Two firmwares in one PlatformIO project:

| Env | Board | Folder |
|---|---|---|
| `head` | Waveshare ESP32-S3-Touch-AMOLED-1.75 | `src/head/` |
| `cam` | ESP32-CAM (AI-Thinker) in the hand box | `src/cam/` |

Docs: [WIRING.md](WIRING.md) · [CALIBRATION.md](CALIBRATION.md)

## Build

```bash
pip install platformio "click<8.2"     # click 8.2+ breaks the bundled esptool
pio run -e head
pio run -e cam
pio run -e head -t buildfs             # web page image (data/)
```
Platform: pioarduino (Arduino core 3.x), LVGL 9.2.2, GFX Library for Arduino 1.5.9, SensorLib 0.3.1,
XPowersLib 0.2.7, ArduinoJson 7.2.1 (all pinned in `platformio.ini`).

## Flash

**Head (USB-C):** hold **BOOT**, tap **PWR/RESET** (or plug in USB while holding BOOT), release BOOT, then
```bash
pio run -e head -t upload
pio run -e head -t uploadfs            # the settings web page
```
Press the reset/PWR button afterwards. Later updates can use OTA: `http://chiku.local/update`.

**ESP32-CAM (USB-TTL adapter):** wire adapter 5V->5V, GND->GND, TX->U0R, RX->U0T, and **IO0 -> GND**.
Press the cam RESET button, then `pio run -e cam -t upload`. Remove IO0-GND and press RESET again.

## First setup

1. Power on. Chiku has no Wi-Fi yet: join the open Wi-Fi **Chiku-Setup** on your phone
   (the setup page opens by itself; if not, go to http://192.168.4.1).
2. Choose your network, enter the password, Save. The head also forwards the credentials to the ESP32-CAM.
3. Open **http://chiku.local** : brightness, volume, location, alarms, timers, servo trims, motion buttons, camera.
4. Time comes from NTP (Europe/Berlin) and is stored in the PCF85063 RTC for offline use.
5. Weather uses Open-Meteo (no key), default Bad Bellingen (47.73, 7.55), refreshed every 15 minutes.

## Using Chiku

* Swipe left/right: Face, Clock, Weather, Timer, Sonar, Camera, Settings.
* Tap: happy/surprised. Long press: menu (gaits, avoid mode, mood demo, calibration, Wi-Fi reset).
* Shake = dizzy. Picked up = surprised. Fall = servos released, sad.
* **Five touch pads** (ears, legs, body), each with single / double / triple / long press:

| Pad | 1 tap | 2 taps | 3 taps | Long press |
|---|---|---|---|---|
| Ear (left or right) | happy, tilts head | excited dance | shy (blush, looks away) | love (sways while held) |
| Leg (left or right) | surprised hop | angry stomp | scared (trembles) | sulk (ruthna: turns away) |
| Body (middle) | happy | jealous (green side-eye) | cry (tears) | love (purr, sways) |

  Chiku also has feelings that build up: petting raises affection; poking a leg again and again makes it
  annoyed, then angry, then it sulks. If ignored for 2 minutes it feels lonely. It does small cute things on
  its own (curious head tilt, yawn, giggle). Edit the table in `behavior.cpp` (`onPad`) to change anything.
* Faces: normal, happy, sad, sleepy, angry, surprised, dizzy, love, thinking, listening, jealous, sulk, shy,
  excited, scared, cry (with tears, blush, sparkles, Zzz). Each feeling also has a servo "body language"
  gait (happy, excited, angry, scared, sulk, jealous, shy, love, sad, surprised, tiltl, tiltr).

## Voice

Chiku speaks with short recorded lines (`data/voice/*.wav`, made by `tools/make_voice.sh` with espeak-ng):
hello, happy, excited, shy, love, surprised, angry, sulk, scared, sad, jealous, alarm, timer, goodmorning.
If a clip is missing it falls back to a synthesised "cute talk" babble that matches the feeling.
Replace a clip with your own voice: web page -> Voice -> Upload (16 kHz, 16-bit, mono WAV with the same name;
ffmpeg command is in `tools/make_voice.sh`). The `uploadfs` step puts the default clips on the head.
Not included: speaking arbitrary text (needs online text-to-speech) and listening (the microphones are not used yet).
* Alarm / timer: ring animation, melody and head shake. Stop with a tap or by shaking.
* After 30 s the screen dims, after 5 min the face sleeps (wake: touch, shake, alarm).

## Hardware self-test

Hold **BOOT** while powering on. Tests display, touch, IMU, RTC, speaker, PCA9685 (each servo moves slowly),
cam link, sonar. The report is shown on screen and on the serial port (115200). Touch the screen to reboot.

## Code map (`src/head/`)

| Module | Job |
|---|---|
| `events` | event bus (queues), the only way modules talk |
| `bus` | shared onboard I2C with a mutex |
| `storage` | NVS settings + LittleFS |
| `display` | CO5300 + LVGL port, PSRAM render buffers |
| `face` | eyes: moods, blink, look, weather effects |
| `ui*` | screen manager and screens |
| `behavior` | reactions (event -> mood + sound + move) |
| `servo` | PCA9685, easing, stagger, idle power-off |
| `motion` | Otto-style gaits, obstacle stop |
| `imu` | shake, pick-up, tilt, tap, fall |
| `touch` | screen gestures (CST9217) |
| `pads` | 5 touch pads: ears, legs, body; tap/double/triple/long |
| `rtc_time`, `alarm_timer`, `weather` | time, alarms/timers/stopwatch, forecast |
| `audio` | ES8311: beeps, melodies, cute-talk babble, WAV voice clips |
| `sonar`, `camlink` | distance filter, UART protocol to the cam |
| `net` | Wi-Fi portal, mDNS, web API, OTA |
| `power` | dim/sleep, CPU frequency, battery |
| `selftest` | BOOT-at-power-on test mode |

Tasks: UI (core 1, prio 3), actuators 50 Hz (core 0, prio 4), sensors (core 0, prio 3), audio (core 0, prio 2),
network (core 0, prio 1). All have the watchdog enabled. All tunables and feature flags: `include/config.h`.

## How to add a new animation (face mood)

1. `src/head/face.h`: add `M_MYMOOD` before `MOOD_COUNT`.
2. `src/head/face.cpp`: add one row at the same position in `MOODS[]` (eye size, lid cover/tilt, colour, ...)
   and a name in `MOOD_NAMES[]`.
3. Trigger it: `face::setMood(face::M_MYMOOD, 2000);` (2 s, then back to the base mood).
4. A new weather effect: add to `enum Fx`, `spawn()` and the mapping in `behavior.cpp::fxFor()`.

## How to add a new gait

1. `src/head/motion.h`: add `G_MYGAIT` before `G_COUNT`.
2. `src/head/motion.cpp`: add a row to `GAITS[]` at the same position:
   `{"name", {amp x4}, {offset x4}, {phase x4}, period_ms}` for
   left hip, right hip, left ankle, right ankle (degrees, phase in degrees).
3. Add the name to `MOVES` in `data/index.html` and to `ui_menu.cpp` (options string + index) if you want a button.
4. Run it with `events::publish(events::EV_MOTION_CMD, motion::G_MYGAIT, cycles);`.

## Camera protocol (UART, JSON lines, 115200)

```
head -> cam  {"cmd":"snap"} {"cmd":"led","on":true} {"cmd":"sleep"} {"cmd":"wake"}
             {"cmd":"ip"} {"cmd":"wifi","ssid":"..","pass":".."}
cam -> head  {"evt":"ready"} {"evt":"ip","ip":"1.2.3.4"} {"evt":"dist","cm":42}
             {"evt":"snap","ok":true,"len":12345} {"evt":"cam","state":"on|sleep"}
```
Stream `http://<cam-ip>/stream`, snapshot `http://<cam-ip>/capture`. The camera powers down after 30 s without a viewer.

## Notes

* Weather HTTPS uses `setInsecure()` (public data, no secrets). Pin a certificate if you prefer.
* Not tested on real hardware when this was written: please run the self-test first and report failures.
