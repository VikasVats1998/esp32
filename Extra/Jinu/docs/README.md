# Jinu

Desk robot on the **Waveshare ESP32-C6-LCD-1.47** (ST7789 172x320, no touch). 4 servos (2 hips, 2 ankles),
speaker through a MAX98357A, HC-SR04 sonar on the front face, 5 touch pads. Same personality as Chiku
(feelings, body language, voice), slimmed down for the C6: one core, no PSRAM, 4 MB flash.

Docs: [WIRING.md](WIRING.md) · [CALIBRATION.md](CALIBRATION.md)

## Build and flash

```bash
pip install platformio "click<8.2"
pio run -d Extra/Jinu -e jinu                 # build
pio run -d Extra/Jinu -e jinu -t upload       # flash over USB-C (hold BOOT, tap RESET if it does not connect)
pio run -d Extra/Jinu -e jinu -t uploadfs     # web page + voice clips (needed for the voice!)
```
Platform: pioarduino (Arduino core 3.x), GFX Library for Arduino 1.6.4 (1.5.x has no C6 SPI driver), ArduinoJson 7.2.1.
Later updates: `http://jinu.local/update` (OTA).

## First setup

Power on. Join the open Wi-Fi **Jinu-Setup** on your phone, pick your network, save. Then open
**http://jinu.local**: faces, feelings, moves, voice, volume, brightness, servo trims.

## Touch pads: gestures and feelings

| Pad | 1 tap | 2 taps | 3 taps | Long press |
|---|---|---|---|---|
| Head (left or right) | happy, leans toward the side | excited dance | shy (blush) | love, sways while held |
| Leg (left or right) | surprised hop | angry stomp | scared (trembles) | sulk (ruthna, turns away) |
| Body (middle) | happy | jealous (green side-eye) | cry (tears) | love |

Feelings build up: petting raises affection; poking the legs again and again makes Jinu annoyed, then angry, then it
sulks. Ignored for 2 minutes it gets lonely. It also does small cute things on its own.
Edit `onPad()` in `src/behavior.cpp` to change anything.

## Other behaviour

* **Sonar:** a hand coming close (under 35 cm) makes Jinu notice and greet you; under 10 cm it stops walking forward.
* **BOOT button:** short press = explore mode on/off (walks and avoids obstacles). Hold BOOT while powering on = self-test.
* **RGB LED** on the board glows in the colour of the current mood (level in `config.h`).
* **Power:** screen dims after 30 s, face sleeps after 5 min (CPU slows to 80 MHz); touch, a hand near, or BOOT wakes it.
* **Voice:** 11 short clips in `data/voice/` (made by `tools/make_voice.sh`); missing clip = cute babble.
  Only about 960 KB of file space exists, so keep your own clips short.
* Not on this board: touch screen, IMU (no shake/fall detection), microphone.

## Code map (`src/`)

| File | Job |
|---|---|
| `events` | event bus (queues) |
| `storage` | NVS settings + LittleFS |
| `display`, `face` | ST7789 canvas, eyes with 16 moods + effects (tears, blush, sparkles, Zzz) |
| `servo`, `motion` | 4 servos with easing/stagger/idle-off; Otto gaits + emotion gaits |
| `pads`, `sonar` | 5 touch pads with tap/double/triple/long, HC-SR04 (interrupt based) |
| `audio` | I2S, beeps, melodies, babble, WAV clips |
| `behavior` | pads/sonar/button -> mood + movement + voice, feelings, idle life |
| `power`, `net`, `selftest`, `main` | dim/sleep, Wi-Fi portal + web + OTA, self-test, tasks |

Tasks (single core): actuator 50 Hz (prio 4), face 30 fps (3), sensors (3), audio (2), network (1); watchdog on.

## Add a face mood / a gait

* Mood: add `M_X` to `enum Mood` (face.h), one row in `MOODS[]` and `NAMES[]` (face.cpp), then `face::setMood(face::M_X, 2000)`.
* Gait: add `G_X` before `G_COUNT` (motion.h) and one row in `GAITS[]` (motion.cpp):
  `{"name", {amp x4}, {offset x4}, {phase x4}, period_ms}` for left hip, right hip, left ankle, right ankle.
  Run it with `events::publish(events::EV_MOTION_CMD, motion::G_X, cycles)`.

## Notes

* Not tested on real hardware when written: run the self-test first (hold BOOT at power-on).
* Flash use is about 81 % of one OTA slot; keep it in mind before adding big libraries.
