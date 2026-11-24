// power.h - power states, CPU frequency, battery (AXP2101).
//   ACTIVE -> DIM after DIM_AFTER_MS -> SLEEP after SLEEP_AFTER_MS without activity (config.h).
// Battery: publishes EV_BATTERY when it gets low (< LOW_BATTERY_PCT), when charging starts and
// when it is fine again; the face shows the UI kit "low battery" / "charging" looks.
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
void sleepNow();                // power menu "Sleep"
uint32_t idleMs();              // time since the last activity
bool lowBattery();
void powerOff();                // AXP2101 switches the board off (deep sleep if the PMU is missing)
}  // namespace power
