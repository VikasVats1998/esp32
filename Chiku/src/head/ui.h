// ui.h - screen manager. One LVGL root with one container per screen; only
// one is visible. Swipe left/right changes screen (order: ui.cpp ORDER[]), long press opens the menu.
// Also draws the timer progress ring over every screen.
#pragma once
#include <Arduino.h>
#include "events.h"

namespace ui {
enum Screen : uint8_t {
  S_FACE, S_CLOCK, S_WEATHER, S_TIMER, S_SONAR, S_CAMERA, S_SETTINGS,   // swipe order
  S_MENU, S_CALIB, S_DESK,
  S_LIGHT, S_DEVICES, S_FAN, S_MOTION, S_SYSTEM,           // hub dashboard (appended: keeps old ids)
  S_COUNT
};
void init();                           // after display::init()
void update(uint32_t dtMs);            // UI task, ~30 fps
void onEvent(const events::Event& e);
void show(Screen s);
Screen current();
void applyBrightness();                // set panel brightness from power state + settings
void setTestPattern(int p);            // 0 off, 1 red, 2 green, 3 blue, 4 white, 5 black, 6 grey ramp (any task)
void startMoodDemo();                  // cycle through all moods on the face
}  // namespace ui
