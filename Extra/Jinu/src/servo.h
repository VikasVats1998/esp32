// servo.h - 4 hobby servos on GPIO0..3 (LEDC PWM, 50 Hz).
// Smooth moves with a speed limit, staggered starts (no current spikes), soft
// start on boot. After 2 s idle the PWM pins are released (no holding current).
// Safe to call from any task: calls only set targets; update() does the work.
#pragma once
#include <Arduino.h>

namespace servo {
void init();
void update(uint32_t dtMs);                       // actuator task, every 20 ms
void setTarget(uint8_t ch, float deg, float speedDps = 0);
void setTargetNow(uint8_t ch, float deg);         // oscillator mode (no easing)
float current(uint8_t ch);
bool moving();
void releaseAll();
void hold(uint8_t ch);                            // output the target now (shows a new trim)
}  // namespace servo
