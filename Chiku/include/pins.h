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
#define PIN_I2C1_SDA  17   // header pin 6  external I2C -> PCA9685 SDA
#define PIN_I2C1_SCL  18   // header pin 7  external I2C -> PCA9685 SCL
#define PIN_TTP223    16   // header pin 8  touch sensor (digital, active HIGH)
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
