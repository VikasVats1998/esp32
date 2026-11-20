# Jinu wiring (Waveshare ESP32-C6-LCD-1.47)

Header pins: top row `5 4 3 2 1 0 3V3 GND 5V`, bottom row `9 18 19 20 23 12 13 RXD TXD`
(RXD = GPIO17, TXD = GPIO16). GPIO9 = BOOT and GPIO12/13 = USB are **not used**.

## Pin budget

Free header GPIOs: 0, 1, 2, 3, 4, 5, 16, 17, 18, 19, 20, 23 = **12**.
Wanted: servos 4 + I2S 3 + sonar 2 + touch 5 = 14. So the five touch sensors share **two I2C pins**
through one **PCF8574** expander (this leaves GPIO16 free for its INT line).

| GPIO | Signal | Connects to |
|---:|---|---|
| 0 | Servo | left hip signal |
| 1 | Servo | right hip signal |
| 2 | Servo | left ankle signal |
| 3 | Servo | right ankle signal |
| 4 | I2S BCLK | MAX98357A BCLK |
| 5 | I2S LRC | MAX98357A LRC |
| 17 (RXD) | I2S DIN | MAX98357A DIN |
| 18 | Sonar TRIG | HC-SR04 TRIG |
| 19 | Sonar ECHO | HC-SR04 ECHO (**5 V sensor: 1 k / 2 k divider**; HC-SR04P 3.3 V: direct) |
| 20 | I2C SDA | PCF8574 SDA |
| 23 | I2C SCL | PCF8574 SCL |
| 16 (TXD) | PCF8574 INT | optional (wired as an input, not needed) |
| 3V3 | | PCF8574 VCC, 5 x TTP223 VCC, MAX98357A SD (if the amp needs it) |
| 5V | | MAX98357A VIN, HC-SR04 VCC |
| GND | | everything (use a GND rail, the board has one GND pin) |

Onboard (do not wire): display MOSI 6, SCLK 7, CS 14, DC 15, RST 21, backlight 22, RGB LED 8.
Do not use the TF card slot (it shares GPIO4/5 with the I2S pins).

## Touch pads (5 x TTP223, OUT is active HIGH)

PCF8574 module at address **0x20** (A0, A1, A2 to GND). Use the PCF8574, not the PCF8574A (0x38).

| Pad | Where | PCF8574 pin |
|---|---|---|
| Head left | left side of the head | P0 |
| Head right | right side of the head | P1 |
| Leg left | left leg | P2 |
| Leg right | right leg | P3 |
| Body | middle of the body (5th sensor) | P4 |

Each TTP223: VCC to 3V3, GND, OUT to the pad pin above. If you want a different fifth location it
is only a mounting choice; nothing in the wiring changes.

## Speaker and amplifier

* Speaker 4 ohm 4 W on the MAX98357A output (+ / -). The MAX98357A gives about 3 W into 4 ohm at 5 V,
  so a 4 W speaker is fine. Keep the volume setting moderate (default 60 %).
* Put 470-1000 uF across the amplifier VIN/GND close to the board.
* The firmware assumes an **I2S amplifier (MAX98357A)** like your earlier sketches. An analog amplifier
  (PAM8403 etc.) would need a different audio driver.

## Servos

* 4 SG90/MG90S. Signal from GPIO0-3; **power from a separate 5 V supply of at least 2 A**, ground shared with the board.
* A 470-1000 uF capacitor on the servo supply. Servos are started one after another and released after 2 s idle.

## Ultrasonic (front face)

HC-SR04 in the two round holes of the front face. 5 V version: put a 1 k (ECHO to GPIO19) and 2 k (GPIO19 to GND)
divider on ECHO. Distance below 10 cm stops walking forward.
