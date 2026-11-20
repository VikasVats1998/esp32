// motion.h - Otto-style gaits. Each gait = per servo: amplitude, offset, phase
// (an "oscillator"), plus a period and a number of cycles. Runs in the actuator
// task and drives servo::setTargetNow(). Safety: stops on fall, and while
// walking forward stops if something is closer than OBSTACLE_STOP_CM.
// To add a gait: add an enum value + one row in GAITS[] (motion.cpp).
#pragma once
#include <Arduino.h>

namespace motion {

enum Gait : uint8_t {
  G_HOME, G_WALK, G_BACK, G_TURN_L, G_TURN_R, G_BEND, G_SHAKE_LEG,
  G_MOONWALK, G_DANCE, G_JUMP, G_WIGGLE, G_COUNT
};

void init();
void update(uint32_t dtMs);              // actuator task, every 20 ms
void start(Gait g, uint8_t cycles = 4);  // any task (queued as a request)
void stop();                             // go home softly
bool busy();
Gait current();
void setAvoidMode(bool on);              // wander and avoid obstacles
bool avoidMode();
const char* gaitName(Gait g);
bool gaitFromName(const char* name, Gait& out);
void lookPan(float x);                   // head pan follows look (-1..1)
}  // namespace motion
