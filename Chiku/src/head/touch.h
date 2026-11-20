// touch.h - CST9217 screen touch (LVGL input device + gestures) and the
// TTP223 pet sensor (debounced). Gestures become events:
//   tap, long press, swipe left/right. TTP223 -> EV_PET.
#pragma once
#include <Arduino.h>

namespace touch {
bool init();                 // UI task, after display::init()
void pollPet();              // call every ~10 ms from the sensor task
bool isDown();               // finger on screen right now
void setGestures(bool on);   // false while a screen needs raw touches
}  // namespace touch
