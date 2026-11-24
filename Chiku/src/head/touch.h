// touch.h - CST9217 screen touch (LVGL input device + gestures). Touch points are turned by the
// screen rotation, so gestures always match what you see. Gestures become events: tap, long press
// (POWER_MENU_HOLD_MS), swipe left/right/up/down. (Body pads: see pads.h.)
#pragma once
#include <Arduino.h>

namespace touch {
enum GestureMode : uint8_t {
  G_ALL,          // tap, long press, all swipes (face and simple screens)
  G_HORIZONTAL,   // tap + left/right swipes only (screens that scroll up/down)
  G_NONE          // only taps (screens with sliders / buttons that are held)
};
bool init();                 // UI task, after display::init()
bool isDown();               // finger on screen right now
uint32_t holdMs();           // how long the finger rests without moving (0 = not touching)
void setGestures(GestureMode m);
}  // namespace touch
