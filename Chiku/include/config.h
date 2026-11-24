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
#define FEATURE_MIC       1   // ES7210 onboard microphones (sound level, loud sound)
#define DEBUG_HEAP_LOG    1   // print free heap/PSRAM every minute

// ---- Identity ----
#define FW_VERSION        "1.2.0"
#define ROBOT_NAME        "chiku"           // mDNS: chiku.local
#define AP_SSID           "Chiku-Setup"     // captive portal (open network)

// ---- Display / face ----
#define FACE_FPS            30
#define FACE_GLOW           14    // glow around the eyes in px (0 = off, saves render time)
#define BLINK_MIN_MS      2000    // UI kit: random blink every 2-6 s
#define BLINK_MAX_MS      6000
#define LOOK_MIN_MS       8000    // UI kit: look-around every 8-15 s
#define LOOK_MAX_MS      15000
#define LOOK_NECK_DEG       10    // the neck follows a look by up to this many degrees
#define IDLE_BREATHE_DEG     3    // tiny neck "breathing" while idle (0 = off, servo rests)
#define EMOTION_HOLD_MS   4000    // emotions return to the resting face after 3-5 s
#define BOOT_TITLE   "TechKalu"   // boot screen logo text
#define BOOT_SUBTITLE  "AI PET"
#define BOOT_MAX_MS       6000    // boot screen waits this long for Wi-Fi at most
#define POWER_MENU_HOLD_MS 3000   // long-press the screen this long: power menu
#define LOW_BATTERY_PCT     15    // below: tired yellow eyes, slower moves
#define BRIGHTNESS_DEFAULT  200   // 0..255
#define BRIGHTNESS_DIM       40
#define BRIGHTNESS_SLEEP      3
#define DIM_AFTER_MS      60000UL
#define BORED_AFTER_MS   300000UL  // UI kit: no input 5 min -> bored
#define SLEEPY_AFTER_MS  540000UL  //           then sleepy ...
#define SLEEP_AFTER_MS   600000UL  //           10 min -> sleep screen (very dim, touch wakes)

// ---- Screen orientation ----
// The round panel can sit at any angle in the head: the picture and the touch input are turned by
// "screen rotation" (0-359 deg, saved in NVS, set in Settings -> Screen orientation or first setup).
// "Auto" measures it with the IMU: gravity in the panel plane. IMU_SCREEN_* map the IMU axes to the
// panel axes (x = right, y = down on the unrotated panel). ASSUMPTION, check once: in Screen orientation
// tilt the head; the level line must stay level. If it turns the wrong way, flip the sign of X here.
#define IMU_SCREEN_X(ax, ay)   (ax)
#define IMU_SCREEN_Y(ax, ay)   (-(ay))
#define ROTATION_DEFAULT        0

// ---- Microphones ----
#define MIC_GAIN            10   // ES7210 gain step 0..14 (3 dB each, 10 = 30 dB)
#define MIC_DB_OFFSET      100   // dBFS + offset = rough dB value shown on screen (not calibrated)
#define MIC_VOICE_DB        55   // above: someone talks (mic check)
#define MIC_LOUD_DB         78   // sudden sound above this: surprised

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
#define LONG_PRESS_MS        700   // touch pads

// ---- Cam link ----
#define CAM_BAUD          115200
#define CAM_STREAM_PORT       80
