// imu.h - QMI8658 motion detection: shake, pick-up, put-down, tilt, tap, fall.
// The orientation at boot (robot standing) is the "upright" reference.
#pragma once
#include <Arduino.h>

namespace imu {
bool init();
void update();                       // call every ~10 ms from the sensor task
bool online();
void getAccel(float& x, float& y, float& z);   // in g
float tiltDeg();                     // angle away from upright
}  // namespace imu
