// config.h - tunables of the remote.
#pragma once
#include "pins.h"

#define FW_VERSION         "1.0.0"
#define RADIO_CHANNEL      1                  // must be the same as in the gimbal module

// ---- Joystick ----
#define JOY_DEADZONE       0.07f              // fraction of full travel that counts as "centre"
#define JOY_EXPO           0.45f              // 0 = linear, 1 = very soft around the centre (cubic curve)
#define JOY_FINE_SCALE     0.45f              // fine mode: all movements scaled down
#define JOY_INVERT_X       0
#define JOY_INVERT_Y       0
#define JOY_SWAP_XY        0
#define JOY_SMOOTHING      0.55f              // 0 = none, 0.9 = very smooth (adds delay)

// ---- Radio ----
#define TX_INTERVAL_MS     20                 // 50 frames per second while something happens
#define TX_IDLE_INTERVAL_MS 100               // 10 per second when the sticks are centred and no key is held
#define LINK_LOST_MS       1500               // no status frame from the gimbal for this long: "no link"

// ---- Buttons ----
#define TAP_WINDOW_MS      300                // wait this long for the next tap (double / triple tap)
#define LONG_PRESS_MS      700
#define PAIR_HOLD_MS       3000               // B1 + B4 held: pairing
#define CALIB_HOLD_MS      3000               // B2 + B3 held: joystick calibration
#define CALIB_TIME_MS      6000

// ---- Power ----
#define SLEEP_AFTER_MS     300000UL           // deep sleep after 5 minutes without any input (any button wakes it)
#define BATT_LOW_MV        3500
#define BATT_FULL_MV       4150
