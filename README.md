# ESP32-C6 EMO Robot

EMO-style desk robot sketches for the **Waveshare ESP32-C6-LCD-1.47** (ST7789 172x320 screen, used in landscape).

| Sketch | What it does |
|---|---|
| [EmoFace](EmoFace/EmoFace.ino) | Animated EMO eyes: blinking, looking around, 8 moods |
| [EmoPro](EmoPro/EmoPro.ino) | Multi-mode showcase: EMO face, Flappy EMO game, light bulb, Matrix rain, plasma, warp starfield, fireworks (BOOT switches modes) |
| [EmoBot](EmoBot/EmoBot.ino) | Walking robot: 4 servos (2 hips + 2 feet), HC-SR04 ultrasonic, 2x TTP223 touch, pet/explore/sleep behaviour |
| [EmoTest](EmoTest/EmoTest.ino) | Cycle through every face with BOOT; each face speaks a phrase through a MAX98357A |
| [HelloVoice](HelloVoice/HelloVoice.ino) | Says "Hello world!" through the MAX98357A |
| [AudioDiag](AudioDiag/AudioDiag.ino) | Tries every I2S pin combination to find a working amplifier wiring |

## Board header

```
 TOP ROW:     [5]  [4]  [3]  [2]  [1]  [0]  [3V3] [GND] [5V]      <- USB-C side
 BOTTOM ROW:  [9] [18] [19] [20] [23] [12] [13]  [RXD] [TXD]
```
RXD = GPIO17, TXD = GPIO16. GPIO9 is BOOT, GPIO12/13 are USB - don't use them.

## Wiring

| Pin | Connect to |
|---|---|
| 0 / 1 | Left hip / right hip servo signal |
| 2 / 3 | Left foot / right foot servo signal |
| 4 | MAX98357A BCLK |
| 5 | MAX98357A LRC |
| RXD (17) | MAX98357A DIN |
| 18 | HC-SR04 TRIG |
| 19 | HC-SR04 ECHO **via 1k/2k voltage divider** (ECHO is 5V) |
| 20 | TTP223 head SIG |
| 23 | TTP223 back SIG |
| 3V3 | TTP223 VCC, MAX98357A SD |
| 5V | MAX98357A VIN, HC-SR04 VCC |
| GND | Everything (use a GND rail - the board has only one GND pin) |

- Power the servos from a separate 5V >= 2A supply, sharing GND with the board.
- MAX98357A: SD must go to 3V3 on many clone boards or the amp stays off. Keep `VOLUME` low for small speakers.
- Don't use the TF card slot together with audio: it shares GPIO4/5.
- EmoBot servo calibration: hold BOOT while powering on, all servos go to 90 deg; adjust `trim[]` / `reverse[]`.

## Build & upload

```sh
arduino-cli core install esp32:esp32
arduino-cli lib install "GFX Library for Arduino"
arduino-cli compile --fqbn esp32:esp32:esp32c6:CDCOnBoot=cdc EmoPro
arduino-cli upload -p /dev/ttyACM0 --fqbn esp32:esp32:esp32c6:CDCOnBoot=cdc EmoPro
```

## Voices

Voice clips are generated offline with [Piper](https://github.com/rhasspy/piper) (`en_US-lessac-high`) and converted to C arrays:

```sh
echo "Hello world!" | piper --model en_US-lessac-high.onnx --output_file hello.wav
python3 EmoTest/tools/make_voices.py HelloVoice/voices.h 1.0 hello=hello.wav
```
