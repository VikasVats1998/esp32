// sonar.h - distance in cm with a median filter.
// Source: samples arrive as EV_DIST_RAW events (the ESP32-CAM reads the
// HC-SR04P and sends them over UART). If PIN_SONAR_TRIG/ECHO are set in pins.h
// the head reads a sensor directly too. Output: EV_DIST and EV_OBSTACLE.
#pragma once
#include <Arduino.h>

namespace sonar {
void init();
void update();                // sensor task, every ~50 ms
int distanceCm();             // filtered, -1 = no reading
uint32_t ageMs();             // time since last sample
}  // namespace sonar
