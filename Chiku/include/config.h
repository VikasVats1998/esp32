// config.h - all tunables and feature flags for the HEAD firmware.
// Set a feature to 0 to remove the module from the build.
#pragma once
#include "pins.h"

// ---- Feature flags ----
#define FEATURE_WEATHER   1
#define FEATURE_ALARM     1
#define FEATURE_IMU       1
#define FEATURE_SERVOS    1
#define FEATURE_SONAR     1
#define FEATURE_CAMLINK   1
#define FEATURE_NET       1
#define FEATURE_AUDIO     1
#define FEATURE_PMU       1
#define FEATURE_SELFTEST  1
#define DEBUG_HEAP_LOG    1   // print free heap/PSRAM every minute

// ---- Identity ----
#define ROBOT_NAME        "chiku"           // mDNS: chiku.local
#define AP_SSID           "Chiku-Setup"     // captive portal (open network)

// ---- Display / face ----
#define FACE_FPS            30
#define BRIGHTNESS_DEFAULT  200   // 0..255
#define BRIGHTNESS_DIM       40
#define BRIGHTNESS_SLEEP      3
#define DIM_AFTER_MS      30000UL
#define SLEEP_AFTER_MS   300000UL

// ---- Weather ----
#define WEATHER_LAT_DEFAULT   47.73f   // Bad Bellingen
#define WEATHER_LON_DEFAULT    7.55f
#define WEATHER_PERIOD_MS  (15UL * 60UL * 1000UL)
#define TIMEZONE_POSIX     "CET-1CEST,M3.5.0,M10.5.0/3"   // Europe/Berlin
#define NTP_SERVER         "pool.ntp.org"

// ---- Servos ----
#define SERVO_FREQ_HZ        50
#define SERVO_US_MIN        500    // pulse at 0 deg
#define SERVO_US_MAX       2400    // pulse at 180 deg
#define SERVO_IDLE_OFF_MS   2000   // release servos after this idle time
#define SERVO_MAX_SPEED_DPS  300.f // deg/s limit
#define SERVO_STAGGER_MS      40   // start delay between servos (avoid current spikes)
#define SERVO_SOFTSTART_DPS   40.f // speed of the first move after boot
// 1 = reverse this servo (use the calibration screen). Gait tables are Otto style:
// the same sign on both ankles leans the body to one side.
#define SERVO_INVERT_DEFAULT { 0, 0, 0, 0, 0 }

// ---- Sensors ----
#define OBSTACLE_STOP_CM      10
#define SONAR_MEDIAN_N         5
#define SHAKE_G_THRESHOLD    2.2f
#define FALL_FREEFALL_G      0.35f
#define TTP223_DEBOUNCE_MS    60
#define LONG_PRESS_MS        700

// ---- Cam link ----
#define CAM_BAUD          115200
#define CAM_STREAM_PORT       80
