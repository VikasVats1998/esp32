// pins.h - every GPIO used by Jinu. See docs/WIRING.md.
// Board: Waveshare ESP32-C6-LCD-1.47. GPIO9 = BOOT, GPIO12/13 = USB (never used).
#pragma once

// ---- Onboard (fixed by the board) ----
#define PIN_LCD_MOSI 6
#define PIN_LCD_SCLK 7
#define PIN_LCD_CS   14
#define PIN_LCD_DC   15
#define PIN_LCD_RST  21
#define PIN_LCD_BL   22      // backlight (PWM)
#define PIN_RGB_LED  8       // onboard WS2812: glows with Jinu's mood
#define PIN_BOOT     9
#define LCD_W 320            // landscape
#define LCD_H 172

// ---- Servos (signal wires; power from a separate 5 V supply) ----
#define PIN_SERVO_LEFT_HIP    0
#define PIN_SERVO_RIGHT_HIP   1
#define PIN_SERVO_LEFT_ANKLE  2
#define PIN_SERVO_RIGHT_ANKLE 3
#define SERVO_LEFT_HIP    0
#define SERVO_RIGHT_HIP   1
#define SERVO_LEFT_ANKLE  2
#define SERVO_RIGHT_ANKLE 3
#define SERVO_COUNT       4

// ---- Speaker amplifier (MAX98357A, I2S) ----
#define PIN_I2S_BCLK 4
#define PIN_I2S_LRC  5
#define PIN_I2S_DIN  17      // header "RXD"

// ---- Ultrasonic HC-SR04 (front face) ----
#define PIN_SONAR_TRIG 18
#define PIN_SONAR_ECHO 19    // 5 V sensor: 1k/2k divider! (HC-SR04P 3.3 V: direct)

// ---- Touch pads: 5x TTP223 on a PCF8574 I2C expander ----
// The header only has 12 free GPIOs; servos(4)+I2S(3)+sonar(2) = 9, so the five
// pads share two I2C pins through one PCF8574 (address 0x20, A0-A2 = GND).
#define PIN_I2C_SDA  20
#define PIN_I2C_SCL  23
#define PIN_PAD_INT  16      // PCF8574 INT (optional, wired but only used as input)
#define I2C_ADDR_PCF8574 0x20
#define PAD_HEAD_L 0         // PCF8574 P0
#define PAD_HEAD_R 1         // P1
#define PAD_LEG_L  2         // P2
#define PAD_LEG_R  3         // P3
#define PAD_BODY   4         // P4 (the 5th sensor, middle of the body)
#define PAD_COUNT  5
