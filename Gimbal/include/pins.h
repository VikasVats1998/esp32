// pins.h - ESP32-S3 Super Mini wiring inside the gimbal. See docs/WIRING.md.
// Avoid GPIO 0, 3, 45, 46 (strapping) and 19/20 (USB).
#pragma once

// ---- Joystick injection (PWM -> RC filter -> joystick wiper node; Hi-Z when the remote is idle) ----
#define PIN_JOY_X_OUT   1
#define PIN_JOY_Y_OUT   2
#define PIN_ZOOM_OUT    4       // only used when ZOOM_ANALOG = 1 (zoom slider is a potentiometer)

// ---- Optional: read the physical joystick wipers (to learn the voltage range). ADC1 pins. -1 = not wired ----
#define PIN_JOY_X_IN   12
#define PIN_JOY_Y_IN   13
#define PIN_ZOOM_IN    14

// ---- Button outputs (HIGH = press: drives a photorelay / optocoupler across the gimbal's button) ----
#define PIN_KEY_SHUTTER  5
#define PIN_KEY_M        6
#define PIN_KEY_TRIGGER  7
#define PIN_KEY_ZOOM_IN  8      // only when the zoom control is two buttons (ZOOM_ANALOG = 0)
#define PIN_KEY_ZOOM_OUT 9
#define PIN_KEY_X       10      // spare

// ---- Status LED (the Super Mini's RGB LED), -1 = none ----
#define PIN_STATUS_LED  48
