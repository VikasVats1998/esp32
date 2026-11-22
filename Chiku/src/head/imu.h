// imu.h - QMI8658 motion detection: shake, pick-up, put-down, tilt, tap, fall.
// The orientation at boot (robot standing) is the "upright" reference.
#pragma once
#include <Arduino.h>

namespace imu {
bool init();
void update();                       // call every ~10 ms from the sensor task
bool online();
void getAccel(float& x, float& y, float& z);   // in g
void getGyro(float& x, float& y, float& z);    // deg/s
float tiltDeg();
float roll();                                  // deg, + = right side down (accel + gyro fused)
float pitch();                                 // deg, + = nose up
bool flipped();                                // upside down
void calibrate();                              // re-learn gyro zero + upright; keep the robot still
bool calibrated();                             // gyro zero learned
float gyroBias(int axis);                     // angle away from upright
}  // namespace imu
