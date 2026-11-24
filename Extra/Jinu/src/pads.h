// pads.h - five TTP223 touch pads (head left/right, leg left/right, body) read
// through a PCF8574 on I2C (GPIO20/23). Each pad gives gestures: 1/2/3 taps or
// a long press. Event: EV_PAD, a = pad id (PAD_*), b = Gesture.
#pragma once
#include <Arduino.h>

namespace pads {
enum Gesture : uint8_t { TAP1 = 1, TAP2 = 2, TAP3 = 3, LONG_START = 4, LONG_END = 5 };
bool init();                // false if the PCF8574 is missing
void update();              // sensor task, every ~10 ms
bool online();
uint8_t rawState();         // bit n = pad n touched now
}  // namespace pads
