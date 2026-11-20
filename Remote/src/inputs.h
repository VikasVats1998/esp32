// inputs.h - joystick and buttons of the remote.
//   B1 capture, B2 zoom in, B3 zoom out: held keys (the gimbal presses its button while you hold yours)
//   B4 (mode): tap = photo/video, double = switch camera, triple = lock, long = ActiveTrack
//   joystick click: tap = recenter, double = flip/selfie, triple = sport, long = fine mode on/off (local)
//   B1 + B4 held 3 s = pairing, B2 + B3 held 3 s = joystick calibration
// Edit the table in inputs.cpp (ACTIONS) to change the mapping.
#pragma once
#include <Arduino.h>

namespace inputs {

struct State {
  int8_t jx, jy;              // shaped joystick -127..127
  bool stickActive;           // outside the dead zone (or just released)
  uint16_t keys;              // remotelink::KeyBit mask to hold
  bool fine;
  uint8_t macro;              // id of the last triggered macro
  uint8_t macroCount;         // increments every time a macro is triggered
  uint32_t lastInputMs;       // millis() of the last stick / button activity
};

enum Event : uint8_t { EV_NONE, EV_PAIR, EV_CALIBRATE, EV_FINE_CHANGED };

void init();                  // pins; measures the stick centre (keep the stick at rest while powering on)
Event update();               // call every ~5 ms
const State& state();
bool calibrating();
void startCalibration();
bool anyButtonDown();
}  // namespace inputs
