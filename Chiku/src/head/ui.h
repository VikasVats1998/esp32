// ui.h - screen manager (UI kit navigation). One LVGL root with one container per screen; only one
// is visible.
//   Face: tap = reaction, swipe left/right = Clock / Weather / Timer carousel, swipe up = radial menu,
//         hold 3 s = power menu (a ring fills while you hold).
//   Any other screen: swipe down = back (lists that scroll: swipe right = back).
// Also draws the timer progress ring over every screen.
#pragma once
#include <Arduino.h>
#include "events.h"

namespace ui {
enum Screen : uint8_t {
  S_FACE, S_CLOCK, S_WEATHER, S_TIMER,                 // carousel (swipe left / right)
  S_MENU,                                              // radial menu (swipe up)
  S_ALARMS, S_MOVES, S_SENSORS, S_SONAR, S_CAMERA, S_NETWORK, S_GAMES, S_DESK, S_SETTINGS,
  S_CALIB, S_ORIENT, S_POWER, S_GOODNIGHT, S_RINGING, S_NOTIFY, S_BOOT, S_SETUP,
  S_LIGHT, S_DEVICES, S_FAN, S_MOTION, S_SYSTEM,       // hub dashboard (ui_dash.cpp), menu ring "Home"
  S_COUNT
};
void init();                           // after display::init()
void update(uint32_t dtMs);            // UI task, ~30 fps
void onEvent(const events::Event& e);
void show(Screen s);
void back();                           // to the parent screen
Screen current();
const char* screenName(Screen s);      // "face", "clock", ... (API / app)
bool screenFromName(const char* n, Screen& out);
void applyBrightness();                // set panel brightness from power state + settings
void setTestPattern(int p);            // 0 off, 1 red, 2 green, 3 blue, 4 white, 5 black, 6 grey ramp (any task)
void startMoodDemo();                  // cycle through all moods on the face (with captions)
void toast(const char* text);          // short message at the bottom for 2.5 s (UI task)
void requestRotation(int deg);         // any task: turn the screen (applied in the UI task)
}  // namespace ui
