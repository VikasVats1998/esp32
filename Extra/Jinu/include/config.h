// config.h - tunables for Jinu.
#pragma once
#include "pins.h"

#define FW_VERSION   "1.1.0"
#define ROBOT_NAME   "jinu"            // mDNS: jinu.local
#define AP_SSID      "Jinu-Setup"      // captive portal (open network)
#define DEBUG_HEAP_LOG 1

// Face / display
#define FACE_FPS            30
#define BRIGHTNESS_DEFAULT 200         // 0..255
#define BRIGHTNESS_DIM      40
#define BRIGHTNESS_SLEEP     4
#define DIM_AFTER_MS     30000UL
#define SLEEP_AFTER_MS  300000UL
#define MOOD_LED_LEVEL      28         // onboard RGB LED brightness (0 = off)

// Servos
#define SERVO_US_MIN        500
#define SERVO_US_MAX       2400
#define SERVO_IDLE_OFF_MS  2000        // release servos after this idle time
#define SERVO_MAX_SPEED_DPS 300.f
#define SERVO_STAGGER_MS     40
#define SERVO_SOFTSTART_DPS  40.f
#define SERVO_INVERT_DEFAULT { 0, 0, 0, 0 }

// Sonar
#define OBSTACLE_STOP_CM     10
#define SONAR_MEDIAN_N        5
#define GREET_NEAR_CM        35
