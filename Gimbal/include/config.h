// config.h - tunables of the gimbal module.
#pragma once
#include "pins.h"

#define FW_VERSION          "1.0.0"
#define RADIO_CHANNEL       1                // both devices must use the same Wi-Fi channel
#define PAIRING_WINDOW_MS   120000UL         // after power-up the remote may pair for 2 minutes

// ---- Link safety ----
#define LINK_TIMEOUT_MS     250              // no valid frame for this long: failsafe (sticks centre, keys released)
#define FAILSAFE_CENTER_MS  150              // drive the sticks to centre this long, then let go (Hi-Z)
#define IDLE_HIZ_MS         300              // remote connected but stick idle for this long: physical joystick takes over
#define MAX_KEY_HOLD_MS     20000            // a key that stays "held" this long is released (stuck remote button)

// ---- Joystick output ----
#define PWM_FREQ_HZ         78125            // 10 bit PWM at 78 kHz, filtered by the RC part (470 ohm + 10 uF)
#define PWM_BITS            10
#define ZOOM_ANALOG         0                // 1: zoom slider is a potentiometer (third PWM), 0: zoom is two buttons

// Default calibration in PWM counts (0..1023 = 0..3.3 V). Use the "learn" command to measure the real values.
#define CAL_DEFAULT_CENTER  512
#define CAL_DEFAULT_MIN     120
#define CAL_DEFAULT_MAX     904

#define KEY_PRESS_ACTIVE_HIGH 1
