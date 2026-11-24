// config.h - tunables and safety limits of the desk module.
#pragma once
#include "pins.h"

#define FW_VERSION       "1.0.0"
#define DEVICE_NAME      "desk"           // mDNS: desk.local
#define AP_SSID          "Desk-Setup"     // setup portal (only while no Wi-Fi is saved)
#define LINK_PORT        80

// ---- Safety ----
#define HOLD_KEEPALIVE_MS     450     // a held UP/DOWN is released this long after the last "move" message
#define MAX_RUN_MS          25000     // one continuous movement never lasts longer than this
#define LOCKOUT_AFTER_MAX_MS  3000    // pause after hitting the limit
#define REVERSE_GAP_MS         200    // pause when switching from UP to DOWN (or back)
#define TAP_MIN_MS              40
#define TAP_MAX_MS            1500
#define PRESET_TAP_MS          250    // press time of M1..M4 for "go to preset"
#define PRESET_HOLD_MAX_MS   30000    // if the desk needs the preset key held: longest hold
#define STOP_KEY_TAP_MS         80    // tap used to stop a preset travel (any key stops most desks)

// ---- Secure link ----
#define PAIRING_WINDOW_MS   180000UL  // after power-up, new clients may pair for 3 minutes
#define MAX_CLIENTS             4
#define AUTH_FAIL_LIMIT         5     // wrong messages ...
#define AUTH_FAIL_WINDOW_MS 60000UL   // ... within this time lock all commands ...
#define AUTH_LOCKOUT_MS     60000UL   // ... for this long

// ---- Height feedback ----
#define DESK_BAUD            9600     // handset cable serial speed (check with the capture tool)
#define HEIGHT_MIN_CM        60.0f    // estimator limits (adjust to your desk)
#define HEIGHT_MAX_CM       125.0f
#define EST_SPEED_CM_S        3.6f    // typical lift speed, used when the display cannot be read
#define SERIAL_FRESH_MS      3000
