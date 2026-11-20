// pins.h - ESP32-S3 Super Mini wiring for the desk module. See docs/WIRING.md.
// Avoid GPIO 0, 3, 45, 46 (strapping), 19/20 (USB). Every button output has a 10 k pull-down to GND
// so nothing is ever pressed while the chip boots or resets.
#pragma once

// ---- Button outputs (HIGH = "press": drives a photorelay / optocoupler across the handset button) ----
#define PIN_KEY_UP    1
#define PIN_KEY_DOWN  2
#define PIN_KEY_M1    4
#define PIN_KEY_M2    5
#define PIN_KEY_M3    6
#define PIN_KEY_M4    7
#define PIN_KEY_S     8      // "S" (memory set) key
#define PIN_KEY_X     9      // the blank 8th key on the handset (function unknown, wire it to find out)

// ---- Feedback inputs (read only!) ----
#define PIN_DESK_RX   11     // handset cable data line controller -> handset (serial, 3.3 V only!)
#define PIN_DESK_TX   12     // reserved (not driven)
// 10 and 13 are spare.

// ---- Status LED (the Super Mini's RGB LED); -1 = none ----
#define PIN_STATUS_LED 48
