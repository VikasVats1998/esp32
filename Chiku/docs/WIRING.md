# Chiku wiring

## Step 1 result: free GPIOs on the head

Source: Waveshare `HARDWARE_REFERENCE.md` and schematic of the ESP32-S3-Touch-AMOLED-1.75.

The 8-pin header has **only 5 usable GPIOs** and **no I2C pins**:

| Header pin | Signal | Use in Chiku |
|---:|---|---|
| 1 | VBUS (5 V) | not used (body has its own 5 V buck) |
| 2 | GND | neck GND |
| 3 | 3V3 | neck 3.3 V (logic for PCA9685, TTP223) |
| 4 | GPIO44 (U0RXD) | UART RX  <- ESP32-CAM TX |
| 5 | GPIO43 (U0TXD) | UART TX  -> ESP32-CAM RX |
| 6 | GPIO17 | external I2C **SDA** (PCA9685) |
| 7 | GPIO18 | external I2C **SCL** (PCA9685) |
| 8 | GPIO16 | TTP223 OUT |

Everything else is used on the board (display 4-7/12/13/38/39, touch 11/40, SD 1-3,
audio 8/9/10/42/45/46, onboard I2C 14/15, USB 19/20, BOOT 0, IMU INT 21, strapping/flash/PSRAM).

### Pin budget (7 signals wanted, 5 pins free)

Wanted: I2C SDA, I2C SCL, CAM TX, CAM RX, TRIG, ECHO, TTP223 = 7. Free: 5.
Rules from the brief were applied in order:

* **(a) Done:** the **ESP32-CAM reads the HC-SR04P** and sends the distance over UART
  (`{"evt":"dist","cm":42}`, 5 per second). Frees TRIG and ECHO -> exactly 5 pins needed.
* (b) PCF8574 expander: **not needed**.

The sonar is therefore wired to the ESP32-CAM (hand box), not to the head. If you later
free two pins, set `PIN_SONAR_TRIG/ECHO` in `include/pins.h` and the head reads it directly.

### I2C address check (0x40 clash)

| Bus | Pins | Devices |
|---|---|---|
| Onboard `Wire` | GPIO15 SDA / GPIO14 SCL | ES8311 0x18, TCA9554 0x20, AXP2101 0x34, **ES7210 0x40**, PCF85063 0x51, CST9217 0x5A, QMI8658 0x6B |
| External `Wire1` | GPIO17 SDA / GPIO18 SCL | **PCA9685 0x40** (alone on the bus) |

The PCA9685 default address 0x40 equals the onboard ES7210. They are on **different buses**,
so there is no clash. (If you ever join the buses, solder PCA9685 pad A0 -> address 0x41.)

The PCA9685 **OE pin is not wired** (no free pin). Servo outputs are switched off with the
PCA9685 "full off" bit and the chip is put to sleep after 2 s idle.

## Neck wires (7 wires, about 15 cm)

| # | Wire | Head header pin | Body side |
|---|---|---|---|
| 1 | GND | 2 | common GND (buck, PCA9685, TTP223, ESP32-CAM) |
| 2 | 3V3 | 3 | PCA9685 VCC, TTP223 VCC |
| 3 | SDA | 6 (GPIO17) | PCA9685 SDA |
| 4 | SCL | 7 (GPIO18) | PCA9685 SCL |
| 5 | TTP223 | 8 (GPIO16) | TTP223 OUT |
| 6 | CAM TX | 4 (GPIO44, head RX) | ESP32-CAM GPIO14 (crossed) |
| 7 | CAM RX | 5 (GPIO43, head TX) | ESP32-CAM GPIO13 (crossed) |

Add 4.7 k pull-ups to 3V3 on SDA/SCL if your PCA9685 board has none (most have 10 k).
The 5 V rail for the servos goes **only** to PCA9685 V+ (from the 5 V / 5 A buck), never to the head.
Connect all GNDs together (buck GND, PCA9685 GND, neck GND, ESP32-CAM GND).

## Body

| Part | Connection |
|---|---|
| PCA9685 V+ | 5 V buck output (5 A) + a 470-1000 uF capacitor close to the board |
| PCA9685 ch0 / ch1 | left hip / right hip |
| PCA9685 ch2 / ch3 | left ankle / right ankle |
| PCA9685 ch4 | head pan (90 = forward, +-90 deg) |
| TTP223 | VCC 3.3 V, GND, OUT -> neck wire 5 (active HIGH) |
| HC-SR04P (3.3 V version) | VCC 3.3 V (from the ESP32-CAM 3V3 pin), GND, TRIG -> CAM GPIO15, ECHO -> CAM GPIO2 |

## ESP32-CAM (AI-Thinker)

| CAM pin | Connection |
|---|---|
| 5V / GND | 5 V buck (separate from servo noise if possible) / GND |
| GPIO13 | head TX (GPIO43)  - UART RX of the cam |
| GPIO14 | head RX (GPIO44)  - UART TX of the cam |
| GPIO15 | HC-SR04P TRIG |
| GPIO2 | HC-SR04P ECHO (3.3 V sensor only! idles low, so boot is not disturbed) |
| GPIO4 | onboard flash LED (controlled by the head) |
| GPIO12, GPIO0 | not used (strapping) |

The cam UART is **not** UART0 (GPIO1/3), so the USB-TTL flashing adapter does not collide with the head.
