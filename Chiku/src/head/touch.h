// touch.h - CST9217 screen touch (LVGL input device + gestures). Gestures
// become events: tap, long press, swipe left/right. (Body pads: see pads.h.)
#pragma once
#include <Arduino.h>

namespace touch {
bool init();                 // UI task, after display::init()
bool isDown();               // finger on screen right now
void setGestures(bool on);   // false while a screen needs raw touches
}  // namespace touch
