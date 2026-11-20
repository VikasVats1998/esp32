// pins.h - ESP32-S3 Super Mini wiring of the handheld remote. See docs/WIRING.md.
// Buttons connect the pin to GND (internal pull-ups are used). All button pins are RTC pins (can wake from deep sleep).
#pragma once

#define PIN_JOY_X      1      // joystick module VRx (ADC1)
#define PIN_JOY_Y      2      // joystick module VRy (ADC1)
#define PIN_JOY_SW     9      // joystick click

#define PIN_BTN_CAPTURE 5     // B1: capture (photo / record)
#define PIN_BTN_ZOOM_IN 6     // B2: zoom in
#define PIN_BTN_ZOOM_OUT 7    // B3: zoom out
#define PIN_BTN_MODE    8     // B4: mode / camera / ActiveTrack (tap, double tap, long press)

#define PIN_BATT       4      // battery voltage through a 100k / 100k divider (ADC1)
#define PIN_BUZZER    10      // optional passive buzzer, -1 = none
#define PIN_STATUS_LED 48     // the Super Mini's RGB LED, -1 = none
