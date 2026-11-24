# OTTO – Waveshare ESP32-C6-LCD-1.47 pinout & wiring

Otto biped with 4 servos, sound, distance sensing and 4 touch pads. Controlled by the display board itself or by the [RemoteJoystick](../RemoteJoystick/PINS.md) over ESP-NOW (the C6 can run the same receiver code; no extra pins needed).

## 1. Pins the board already uses – do NOT connect anything

| GPIO | Used for |
|---|---|
| 6, 7 | LCD / SD SPI (MOSI, SCLK) |
| 14, 15, 21, 22 | LCD CS, DC, RST, backlight |
| 8 | Onboard RGB LED (strapping pin) |
| 12, 13 | USB D− / D+ (needed for flashing) |

## 2. Full pin plan (13 signals)

| # | Module | Module pin | ESP32-C6 GPIO | Note |
|---|---|---|---|---|
| 1 | MAX98357A | BCLK | **18** | I2S |
| 2 | MAX98357A | LRC (WS) | **19** | I2S |
| 3 | MAX98357A | DIN | **20** | I2S |
| – | MAX98357A | VIN / GND | 5V / GND | Leave SD and GAIN unconnected (if the amp stays silent, SD → 3V3) |
| 4 | HC-SR04 | TRIG | **23** | |
| 5 | HC-SR04 | ECHO | **0** | **Voltage divider 1k + 2k** (ECHO is 5V) |
| 6 | TTP223 #1 | OUT | **1** | Head, powered from 3V3 |
| 7 | TTP223 #2 | OUT | **2** | Back |
| 8 | TTP223 #3 | OUT | **3** | Left side |
| 9 | TTP223 #4 | OUT | **4** | Right side – only free if the SD card is unused |
| 10 | Servo #1 | Signal | **9** | Left hip (BOOT pin, see notes) |
| 11 | Servo #2 | Signal | **5** | Right hip – only free if the SD card is unused |
| 12 | Servo #3 | Signal | **17** | Left foot (UART RX) |
| 13 | Servo #4 | Signal | **16** | Right foot (UART TX – fine, serial goes over USB) |

Servo order: #1 left hip, #2 right hip, #3 left foot, #4 right foot. (If you change the order, edit the pin defines in the sketch.)

### Servo types (actual hardware)

| Servo | Position | GPIO | Type |
|---|---|---|---|
| #1 | Left hip | 9 | Blue plastic-gear servo (SG90 class) |
| #2 | Right hip | 5 | Blue plastic-gear servo (SG90 class) |
| #3 | Left foot | 17 | Metal-gear servo (MG90S class) |
| #4 | Right foot | 16 (TXD) | Metal-gear servo (MG90S class) |

Metal-gear servos are stronger but draw more current (roughly 0.5–0.7 A each when loaded or stalled, vs ~0.25 A for the plastic ones), so budget the 5V supply for at least 2–3 A. Their horns are also stiffer to turn by hand when powered; turn them only while they are FREE.

## 3. Board header (as printed)

```
 USB-C side
 TOP ROW:     [5]  [4]  [3]  [2]  [1]  [0]  [3V3] [GND] [5V]
 BOTTOM ROW:  [9] [18] [19] [20] [23] [12] [13]  [RXD] [TXD]
                                                  =17    =16
```
GPIO12/13 are USB – leave unconnected. The board has only one GND pin, so use a GND rail/breadboard strip for everything.

## 4. Wiring diagram

```
                         ┌──────────────────────────────┐
                         │ Waveshare ESP32-C6-LCD-1.47  │
                         │        (1.47" screen)        │
                         └──────────────────────────────┘
   TOP ROW      GPIO5  GPIO4  GPIO3  GPIO2  GPIO1  GPIO0   3V3    GND    5V
                  │      │      │      │      │      │      │      │      │
              Servo2  TTP#4  TTP#3  TTP#2  TTP#1  ECHO*     │      │      │
              (RH)   (right)(left) (back) (head)    │       │      │      │
   BOTTOM ROW   GPIO9  GPIO18 GPIO19 GPIO20 GPIO23 [12][13] RXD=17 TXD=16
                  │      │      │      │      │            │      │
              Servo1   BCLK    LRC    DIN    TRIG        Servo3  Servo4
              (LH)                                        (LF)    (RF)
                       └───── MAX98357A ─────┘   └ HC-SR04 ┘
```

### Power distribution (use rails)

```
   5V SERVO SUPPLY (≥ 2-3 A)                    ESP32-C6 board
   ┌───────────────┐                       ┌──────────────────┐
   │  +5V ─────────┼──► Servo 1-4 red      │ 5V  ──► MAX98357A VIN, HC-SR04 VCC
   │               │                       │ 3V3 ──► TTP223 #1-4 VCC
   │  GND ─────────┼──► Servo 1-4 brown ───┤ GND ──► GND RAIL (everything)
   └───────────────┘      (common GND!)    └──────────────────┘
```
- Servo +5V comes **only** from the separate supply. Do **not** power the servos from the board's 5V pin.
- The supply's GND **must** be joined to the ESP32 GND, otherwise the servos jitter or don't move.
- A 470–1000 µF capacitor across the servo supply helps with brownouts.

### Servo (×4)

```
 Servo wire    Colour (typ.)      Goes to
 Signal        orange / yellow    GPIO 9 / 5 / 17 / 16  (LH / RH / LF / RF)
 V+            red                external 5V supply +
 GND           brown / black      common GND rail
```

### HC-SR04 ultrasonic (ECHO needs a divider)

```
 HC-SR04
 VCC  ───────────────────────── 5V
 TRIG ───────────────────────── GPIO23
 ECHO ──── 1 kΩ ────┬────────── GPIO0
                      │
                     2 kΩ
                      │
 GND  ────────────────┴────────── GND rail
```
ECHO → 1 kΩ → **junction** → GPIO0, and junction → 2 kΩ → GND. The junction sees 5 V × 2/3 ≈ 3.3 V.
An HC-SR04P / RCWL-1601 runs on 3.3 V and needs no divider.

### MAX98357A amplifier

```
 MAX98357A      ESP32-C6
 VIN  ───────── 5V
 GND  ───────── GND rail
 BCLK ───────── GPIO18
 LRC  ───────── GPIO19
 DIN  ───────── GPIO20
 SD   ───────── (unconnected; if silent, tie to 3V3)
 GAIN ───────── (unconnected)
 +/-  ───────── speaker (4-8 Ω)
```

### TTP223 touch pads (×4)

```
 TTP223         ESP32-C6
 VCC  ───────── 3V3
 GND  ───────── GND rail
 OUT  ───────── GPIO1 (head) / 2 (back) / 3 (left) / 4 (right)
```
Output is HIGH when touched.

## 5. Notes & gotchas

1. **GPIO9 is the BOOT pin.** The chip must see it HIGH at reset to boot normally. The servo on GPIO9 (left hip) can pull it low and force download mode. Fix: add a **10 kΩ resistor from GPIO9 to 3V3**, or unplug that servo while flashing/resetting.
2. **GPIO4 and GPIO5 are the SD card's CS and MISO.** If you want to use the SD card, you can't use TTP #4 and Servo #1 as planned.
3. **Cleaner alternative: PCA9685 servo board.** It drives all 4 servos from 2 pins (SDA = GPIO16, SCL = GPIO17), freeing GPIO5 and GPIO9. Power its V+ from the 5V servo supply, VCC from 3V3, and share GND.
4. GPIO16/17 are UART pins, but serial debugging goes over USB so they're free.
5. Check the labels on your board's header and confirm every GPIO is actually brought out before soldering.
6. Do not use GPIO8 (RGB LED) or GPIO12/13 (USB).
7. Keep speaker volume low on small speakers, and keep amp/servo wires away from each other to reduce noise.

## 6. Quick pin cheat sheet (for code)

```cpp
#define PIN_I2S_BCLK  18
#define PIN_I2S_LRC   19
#define PIN_I2S_DIN   20
#define PIN_TRIG      23
#define PIN_ECHO       0   // via 1k/2k divider
#define PIN_TOUCH_HEAD 1
#define PIN_TOUCH_BACK 2
#define PIN_TOUCH_L    3
#define PIN_TOUCH_R    4   // SD card unused
#define PIN_SERVO_LH   9   // BOOT pin: add 10k pull-up to 3V3
#define PIN_SERVO_RH   5   // SD card unused
#define PIN_SERVO_LF  17
#define PIN_SERVO_RF  16   // TXD
```
