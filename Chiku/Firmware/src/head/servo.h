// servo.h - PCA9685 servo driver on the external I2C bus (Wire1, GPIO16/17).
// Each servo glides to its target with a speed limit and easing. Starts are
// staggered (never all servos at once) to avoid current spikes. After 2 s with
// no movement all outputs are switched fully off and the chip is put to sleep.
// All functions are safe to call from any task (they only set targets).
#pragma once
#include <Arduino.h>

namespace servo {
bool init();                              // finds PCA9685, no servo moves yet
void update(uint32_t dtMs);               // call every 20 ms from the actuator task
void setTarget(uint8_t ch, float deg, float speedDps = 0);   // 0 = default speed
void setTargetNow(uint8_t ch, float deg); // oscillator mode: follow without easing
float current(uint8_t ch);                // last commanded angle (degrees, before trim)
bool moving();
void releaseAll();                        // outputs off now (e.g. after a fall)
void freeze();                            // stop where we are and release
bool online();
void applySettings();                     // re-read trims/limits from storage
void hold(uint8_t ch);                    // re-output the current target now (shows a new trim)
void rawPulse(uint8_t ch, float deg);     // calibration: ignores trim/limits
}  // namespace servo
