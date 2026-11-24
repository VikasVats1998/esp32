// pads.h - five TTP223 touch pads: left ear, right ear, left leg, right leg
// (PCF8574 expander) and body (GPIO16). Each pad is debounced and reports a
// gesture: 1 / 2 / 3 taps, or a long press (start and end).
// Event: EV_PAD, a = pad id (PAD_*), b = gesture (Gesture below).
#pragma once
#include <Arduino.h>

namespace pads {
enum Gesture : uint8_t { TAP1 = 1, TAP2 = 2, TAP3 = 3, LONG_START = 4, LONG_END = 5 };
bool init();                 // returns false if the PCF8574 is missing (body pad still works)
void update();               // sensor task, every ~10 ms
bool expanderOnline();
uint8_t rawState();          // bit n = pad n touched right now
const char* padName(uint8_t pad);
}  // namespace pads
