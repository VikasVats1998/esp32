// power.h - power states, CPU frequency, battery (AXP2101).
//   ACTIVE -> DIM after 30 s -> SLEEP after 5 min without activity.
// Any touch / shake / alarm calls activity(), which wakes the robot.
#pragma once
#include <Arduino.h>

namespace power {
enum State : uint8_t { ACTIVE, DIM, SLEEP };
void init();
void update();                  // UI task, about 5 times a second
void activity();                // call on any user interaction
State state();
int batteryPercent();           // -1 = no battery
bool charging();
bool pmuOnline();
float batteryVolts();
void keepAwake(bool on);        // alarm ringing etc: never sleep
}  // namespace power
