# pixel_plus

STILL WORKING ON OMNIBOT INTEGRATION!

Open-source firmware for **Pixel Plus** — an ESP32-S3 desk companion with an animated face, touch UI, voice control, and optional leg servos. Built for the [Waveshare ESP32-S3 Touch AMOLED 1.75"](https://www.waveshare.com/esp32-s3-touch-amoled-1.75.htm) board.

**Repository:** [github.com/nazirlouis/pixel_plus](https://github.com/nazirlouis/pixel_plus)

## Layout

| Directory | Role |
|-----------|------|
| [`pixel_plus_display`](pixel_plus_display) | Main firmware: LVGL touch UI, battery/PMIC, PCA9685 servos, synthesized audio, and on-device speech (WakeNet + MultiNet). |

### Source modules (`pixel_plus_display/main`)

| File | Role |
|------|------|
| `main.c` | LVGL UI, screens, gestures, emotes, emotions, alarm/clock, sleep mode, voice command handlers |
| `speech_wake.c` | WakeNet9 + MultiNet7 pipeline, wake word detection, command registration |
| `pixel_voice.c` | I2S synthesized chirps, motion sounds, and melodic command confirmations |
| `robot_sound.c` | Maps voice-command IDs to `pixel_voice` phrases |
| `pca9685.c` | 4-channel servo driver over I2C (GPIO 17 SDA, GPIO 18 SCL) |
| `battery_pmu.cpp` | AXP2101 battery gauge and charging status via XPowersLib |
| `lv_font_clock_110.c` | Custom 110 pt LVGL font for the clock screen |

## Hardware

A full bill of materials (BOM) is in the repository root: [`BOM - Pixel Plus.xlsx`](BOM%20-%20Pixel%20Plus.xlsx).

### Required

| Item | Notes |
|------|-------|
| [Waveshare ESP32-S3 Touch AMOLED 1.75"](https://www.waveshare.com/esp32-s3-touch-amoled-1.75.htm) | 466×466 AMOLED, touch, on-board mic/speaker, AXP2101 PMU |
| USB-C cable | Flash, monitor, and power |

### Optional servos (PCA9685)

Motion and emotes use a **PCA9685** 16-channel PWM driver at I2C address **`0x40`**. The firmware uses four channels:

| PCA9685 channel | Servo role |
|-----------------|------------|
| 0 | Left leg |
| 1 | Left foot |
| 2 | Right leg |
| 3 | Right foot |

| Signal | ESP32-S3 GPIO |
|--------|---------------|
| SDA | 17 |
| SCL | 18 |

Connect the PCA9685 **SDA/SCL/VCC/GND** to the board’s I2C breakout (3.3 V logic). Power servos from a **separate 5 V supply**; tie grounds together. Use the **Settings → Servos** screen to tune each channel and save center positions to NVS.

If the PCA9685 is missing or fails init, the UI and speech stack still run; only physical motion is disabled (see serial log for `PCA9685 init failed`).

## Features

### Face UI

- Animated eyes with emotion presets: **Idle**, **Happy**, **Confused**, **Serious**
- Touch emotes with foot-servo choreography: lean left/right, dizzy left/right, dance, walk forward/backward (alternating foot plant/lift and hip shift, six steps per walk)
- Additional movement commands are available by voice only (turn, march, spin, jump, sneak, patrol, get back); see Speech below
- Servo ramping with ease-out for smooth physical motion

### Screens

| Screen | Access | Purpose |
|--------|--------|---------|
| Face | Default | Animated eyes and emotes |
| Clock | Swipe left from Face | Set and display time |
| Timer / alarm | Swipe left again | Countdown timer with audible alert |
| Settings | Swipe up from Face | Brightness, speaker volume, battery status |
| Emotions | Settings → Emotions | Swipe left/right to cycle presets |
| Emotes | Settings → Emotes | Trigger lean, dizzy, dance, and walk forward/backward animations |
| Servos | Settings → Servos | Per-channel tuning; center positions saved to NVS |

**Gestures:** swipe up/down to open/close settings; swipe left/right on the Face/Clock/Timer carousel or on the Emotions picker.

### Audio

All sound is **procedurally generated** on the board speaker (no audio files on flash):

- Boot **“wake up”** fanfare and a **listen** chirp when the wake word is detected
- Each recognized voice command plays a short melodic confirmation (via `robot_sound.c` → `pixel_voice.c`)
- Motion chirps during walks, turns, spins, dance, and landings
- Timer alarm tone with repeat pattern

Volume is adjustable in **Settings** and persisted for the session.

### Speech

Wake word **“Jarvis”** (WakeNet9 `wn9_jarvis_tts`), then speak an English command (MultiNet7). Commands are registered in `speech_wake.c` and handled in `main.c`. You have about six seconds after the wake word before the command window times out. **16 commands** are supported.

| Say this (after “Jarvis”) | Action |
|---------------------------|--------|
| “what time is it” | Open the clock screen |
| “start the timer” | Open the timer screen and start a countdown using the current H/M/S picker values (default 0:01:00; adjust rollers on the Timer screen first) |
| “stop the timer” | Cancel the active countdown |
| “go to sleep” / “low power mode” | Sleep: stop motion, disarm timer, park servos, closed eyes, screen off; still listens for Jarvis |
| “do a dance” | Happy face + dance emote (~5 s) |
| “are you dizzy” | Random dizzy emote, then confused expression (~8 s) |
| “walk forward” | Happy face + walk-forward emote (six gait steps) |
| “walk backward” | Happy face + walk-backward emote (six gait steps) |

### Movement commands (voice only)

| Say this (after “Jarvis”) | Action |
|---------------------------|--------|
| “turn left” | Serious face + waddle pivot left (four lean/swing cycles) |
| “turn right” | Serious face + waddle pivot right (four lean/swing cycles) |
| “march” | Serious face + alternating foot lifts in place (ten steps, no hip twist) |
| “do a jump” | Happy face + crouch then both feet snap up (~30°) |
| “sneak around” | Serious face + slow quiet walk forward (eight steps) |
| “get back” | Confused face + frantic backward flee (fourteen fast steps) |
| “do a spin” | Happy face + spinning eyes + fast lean left, left leg +15° forward then −30° back (single move) |
| “patrol” | Serious face + walk forward, turn right, turn left, walk forward |

Speech models load from the `model` SPIFFS partition. AFE runs in low-cost mode with models in PSRAM to preserve internal RAM for display DMA. Wake-word sensitivity is set in `speech_wake.c` (`SPEECH_WN_THRESHOLD`, default 0.55).

### Power

Battery percentage and charging state via the on-board **AXP2101** PMU (XPowersLib).

#### Sleep / low power

Say **“Jarvis”**, then **“go to sleep”** or **“low power mode”**. Pixel will:

- Stop voice motion and touch emotes, and park servos to their saved center positions
- Disarm an active countdown timer (and stop ringing if the timer was alerting)
- Show closed eyes on the face screen, then turn the display backlight off
- Keep the wake-word pipeline running so you can still say **“Jarvis”** without tapping the screen

**Wake:** tap the face screen, or say **“Jarvis”** (backlight restores to the previous brightness; then speak a command as usual). Saying sleep again while already asleep does nothing.

Wi-Fi and other unused stacks are disabled in `sdkconfig.defaults` to free internal RAM for display DMA and ESP-SR.

## Requirements

- [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/) **5.5.x** (developed and tested with **5.5.4**)
- Target: **esp32s3**
- USB cable for flash/monitor
- Optional: PCA9685 PWM driver at I2C address `0x40` (SDA GPIO 17, SCL GPIO 18) for leg/foot servos

### Managed dependencies

Pulled automatically by the ESP-IDF Component Manager on first build (`pixel_plus_display/main/idf_component.yml`):

| Component | Version | Purpose |
|-----------|---------|---------|
| [waveshare/esp32_s3_touch_amoled_1_75](https://components.espressif.com/components/waveshare/esp32_s3_touch_amoled_1_75) | ^2.0.6 | Board BSP, display, touch, codec |
| [cube32esp/xpowerslib](https://components.espressif.com/components/cube32esp/xpowerslib) | ^0.3.3 | AXP2101 battery/PMIC |
| [espressif/esp-sr](https://components.espressif.com/components/espressif/esp-sr) | ^2.4.5 | WakeNet9 + MultiNet7 speech models |

`managed_components/` is gitignored; run `idf.py build` once after cloning to populate it.

## Build and flash

From the display project root:

```bash
cd pixel_plus_display
idf.py set-target esp32s3
idf.py build flash monitor
```

After a clean clone, run `idf.py build` once so the component manager populates `managed_components/` (gitignored). The first build also stages ESP-SR models into the `model` partition via `esp-sr`.

The application binary and ELF are named `pixel_plus_display` (for example `build/pixel_plus_display.elf` for GDB or `idf.py monitor`).

### Windows

Two Windows-specific fixes are applied automatically during configure:

1. [`pixel_plus_display/tools/python.cmd`](pixel_plus_display/tools) is prepended to `PATH` so `esp-sr`’s `movemodel.py` resolves `python` to the ESP-IDF virtualenv.
2. [`pixel_plus_display/cmake/patch_esp_sr_movemodel.cmake`](pixel_plus_display/cmake/patch_esp_sr_movemodel.cmake) patches `movemodel.py` for cp1252 console encoding when the component manager restores `managed_components/`.

If model copy fails, ensure `IDF_PYTHON_ENV_PATH` is set (normal when using the ESP-IDF PowerShell/CMD export script).

## Partitions

Custom layout in [`pixel_plus_display/partitions.csv`](pixel_plus_display/partitions.csv):

| Partition | Purpose |
|-----------|---------|
| `factory` | Application (~2.5 MB) |
| `model` | ESP-SR wake/command models (SPIFFS, 5 MB) |
| `storage` | BSP/assets (SPIFFS) |

Defaults and speech model selection live in [`pixel_plus_display/sdkconfig.defaults`](pixel_plus_display/sdkconfig.defaults) (`CONFIG_SR_WN_WN9_JARVIS_TTS`, `CONFIG_SR_MN_EN_MULTINET7_QUANT`, performance/LVGL trims, Wi-Fi off).

## IDE

Open [`pixel_plus.code-workspace`](pixel_plus.code-workspace) in Cursor or VS Code so paths and the ESP-IDF extension match this layout.

## Contributing

Contributions are welcome. For bugs and feature ideas, open a [GitHub issue](https://github.com/nazirlouis/pixel_plus/issues). For code changes:

1. Fork the repository and create a branch from `main`.
2. Make focused changes; match existing style in the files you touch.
3. Build and flash on hardware (or note what you could not test).
4. Open a pull request with a short description of the change and how you tested it.

## License

This project is open source under the **MIT License**. See [LICENSE](LICENSE) for the full text.

Third-party components (Waveshare BSP, Espressif ESP-SR, XPowersLib, LVGL, and others pulled in by ESP-IDF) remain under their respective licenses.
