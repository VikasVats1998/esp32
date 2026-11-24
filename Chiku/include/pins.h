// pins.h - every GPIO / I2C address used by Chiku. See docs/WIRING.md.
// Rule: never use flash/PSRAM pins (26-37), strapping pins (0,3,45,46 are used
// only by the board itself) or USB pins 19/20.
#pragma once

// ---------------- HEAD: Waveshare ESP32-S3-Touch-AMOLED-1.75 (onboard) ----------
// Display CO5300 (QSPI)
#define PIN_LCD_SDIO0 4
#define PIN_LCD_SDIO1 5
#define PIN_LCD_SDIO2 6
#define PIN_LCD_SDIO3 7
#define PIN_LCD_SCLK  38
#define PIN_LCD_CS    12
#define PIN_LCD_RST   39
#define LCD_W 466
#define LCD_H 466
// Onboard I2C bus (touch, IMU, RTC, PMU, audio codecs)
#define PIN_I2C0_SDA  15
#define PIN_I2C0_SCL  14
#define PIN_TP_INT    11
#define PIN_TP_RST    40
// Audio (ES8311 speaker codec)
#define PIN_I2S_MCLK  42
#define PIN_I2S_BCLK  9
#define PIN_I2S_WS    45
#define PIN_I2S_DOUT  8
#define PIN_I2S_DIN   10
#define PIN_PA_CTRL   46
// Buttons
#define PIN_BOOT      0

// Onboard I2C addresses (7-bit)
#define I2C_ADDR_ES8311 0x18
#define I2C_ADDR_ES7210 0x40   // <- same as PCA9685 default! That is why the
                               //    PCA9685 is on its OWN bus (Wire1) below.
#define I2C_ADDR_PCF85063 0x51
#define I2C_ADDR_CST9217  0x5A
#define I2C_ADDR_QMI8658  0x6B
#define I2C_ADDR_AXP2101  0x34
#define I2C_ADDR_TCA9554  0x20

// ---------------- HEAD: 8-pin header (only 5 free GPIOs!) -----------------------
// Header: 1 VBUS, 2 GND, 3 3V3, 4 GPIO44, 5 GPIO43, 6 GPIO17, 7 GPIO18, 8 GPIO16
#define PIN_CAM_RX    44   // header pin 4  <- ESP32-CAM TX (GPIO14)
#define PIN_CAM_TX    43   // header pin 5  -> ESP32-CAM RX (GPIO13)
#define PIN_I2C1_SDA  16   // external I2C -> PCA9685 SDA (found by the ServoTest pin hunt)
#define PIN_I2C1_SCL  17   // external I2C -> PCA9685 SCL
#define PIN_TTP223    18   // body (middle) touch pad, active HIGH (moved from 16: that is now the PCA9685 SDA)
// HC-SR04P: no free header pin left -> it is wired to the ESP32-CAM, which
// sends the distance over UART (plan "a"). Set both to a GPIO number to read
// the sonar directly on the head instead (-1 = not wired).
#define PIN_SONAR_TRIG (-1)
#define PIN_SONAR_ECHO (-1)
// PCA9685 OE pin is not wired (no spare pin) -> outputs are switched off with
// the PCA9685 "full off" bit instead.
#define PIN_PCA_OE (-1)
#define I2C_ADDR_PCA9685 0x40

// ---------------- ESP32-CAM (AI-Thinker) ---------------------------------------
#define CAM_PIN_LINK_RX  13   // <- head TX
#define CAM_PIN_LINK_TX  14   // -> head RX
#define CAM_PIN_TRIG     15   // HC-SR04P TRIG
#define CAM_PIN_ECHO     2    // HC-SR04P ECHO (3.3 V version, idles low = OK for strap pin)
#define CAM_PIN_FLASH    4    // onboard white LED

// ---------------- Servo channels on the PCA9685 ---------------------------------
#define SERVO_LEFT_HIP    0
#define SERVO_RIGHT_HIP   1
#define SERVO_LEFT_ANKLE  2
#define SERVO_RIGHT_ANKLE 3
#define SERVO_HEAD_PAN    4
#define SERVO_COUNT       5
// Logical servo -> PCA9685 channel as wired: right foot 1, right hip 2, left foot 3, left hip 4, neck 5.
static const unsigned char SERVO_HW_CH[SERVO_COUNT] = {4, 2, 3, 1, 5};   // index = SERVO_LEFT_HIP, RIGHT_HIP, LEFT_ANKLE, RIGHT_ANKLE, HEAD_PAN

// ---------------- Touch pads (5x TTP223, active HIGH) ---------------------------
// Body pad: GPIO16 directly. Ears and legs: PCF8574 I2C expander on the external
// bus (same SDA/SCL wires as the PCA9685), address 0x20 (A0-A2 to GND).
#define I2C_ADDR_PCF8574 0x20
#define PAD_EAR_L  0
#define PAD_EAR_R  1
#define PAD_LEG_L  2
#define PAD_LEG_R  3
#define PAD_BODY   4
#define PAD_COUNT  5
// PCF8574 bit for each pad (P0..P3); the body pad is GPIO16.
#define PCF_BIT_EAR_L 0
#define PCF_BIT_EAR_R 1
#define PCF_BIT_LEG_L 2
#define PCF_BIT_LEG_R 3
