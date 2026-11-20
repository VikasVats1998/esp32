// ui.cpp - see ui.h
#include "ui.h"
#include "config.h"
#include "ui_screens.h"
#include "face.h"
#include "display.h"
#include "touch.h"
#include "power.h"
#include "storage.h"
#include "alarm_timer.h"

namespace ui {

static lv_obj_t* screens[S_COUNT];
static Screen cur = S_FACE;
static lv_obj_t* ring = nullptr;          // timer progress ring on the top layer
static uint32_t slowAcc = 0;              // for the 4 Hz screen updates
static bool demoOn = false;
static uint32_t demoAcc = 0;
static uint8_t demoIdx = 0;

Screen current() { return cur; }

void applyBrightness() {
  uint8_t b = storage::get().brightness;
  switch (power::state()) {
    case power::ACTIVE: display::setBrightness(b); break;
    case power::DIM:    display::setBrightness(min<int>(b, BRIGHTNESS_DIM)); break;
    case power::SLEEP:  display::setBrightness(BRIGHTNESS_SLEEP); break;
  }
}

void show(Screen s) {
  if (s >= S_COUNT) return;
  if (cur == S_CALIB && s != S_CALIB) ui_calib::onHide();
  for (int i = 0; i < S_COUNT; i++) {
    if (i == s) lv_obj_remove_flag(screens[i], LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(screens[i], LV_OBJ_FLAG_HIDDEN);
  }
  cur = s;
  touch::setGestures(s != S_MENU && s != S_CALIB);
  if (s == S_MENU) ui_menu::onShow();
  if (s == S_CALIB) ui_calib::onShow();
  if (s != S_FACE) demoOn = false;
  slowAcc = 1000;                          // refresh the new screen at once
}

void init() {
  lv_obj_t* root = lv_screen_active();
  lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);
  screens[S_FACE] = face::create(root);
  screens[S_CLOCK] = ui_clock::create(root);
  screens[S_WEATHER] = ui_weather::create(root);
  screens[S_TIMER] = ui_timer::create(root);
  screens[S_SONAR] = ui_sonar::create(root);
  screens[S_CAMERA] = ui_camera::create(root);
  screens[S_SETTINGS] = ui_settings::create(root);
  screens[S_MENU] = ui_menu::create(root);
  screens[S_CALIB] = ui_calib::create(root);

  ring = lv_arc_create(lv_layer_top());
  lv_obj_set_size(ring, LCD_W, LCD_H);
  lv_obj_center(ring);
  lv_obj_remove_style(ring, nullptr, LV_PART_KNOB);
  lv_obj_remove_flag(ring, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_arc_opa(ring, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_arc_width(ring, 8, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(ring, lv_color_hex(0xFFA020), LV_PART_INDICATOR);
  lv_arc_set_rotation(ring, 270);
  lv_arc_set_bg_angles(ring, 0, 360);
  lv_arc_set_range(ring, 0, 1000);
  lv_obj_add_flag(ring, LV_OBJ_FLAG_HIDDEN);
  show(S_FACE);
}

void startMoodDemo() { demoOn = true; demoIdx = 0; demoAcc = 0; face::setMood(face::M_NORMAL, 0); }

static void updateRing() {
  int slot = alarm_timer::activeTimer();
  if (slot < 0) { lv_obj_add_flag(ring, LV_OBJ_FLAG_HIDDEN); return; }
  uint32_t total = alarm_timer::timerTotalMs(slot);
  uint32_t left = alarm_timer::timerRemainingMs(slot);
  lv_arc_set_value(ring, total ? (int)(1000UL * left / total) : 0);   // shrinks as time runs out
  lv_obj_remove_flag(ring, LV_OBJ_FLAG_HIDDEN);
}

void update(uint32_t dtMs) {
  if (cur == S_FACE) face::update(dtMs);
  slowAcc += dtMs;
  if (slowAcc >= 250) {
    slowAcc = 0;
    switch (cur) {
      case S_CLOCK: ui_clock::update(); break;
      case S_WEATHER: ui_weather::update(); break;
      case S_TIMER: ui_timer::update(); break;
      case S_SONAR: ui_sonar::update(); break;
      case S_CAMERA: ui_camera::update(); break;
      case S_SETTINGS: ui_settings::update(); break;
      default: break;
    }
    static uint8_t ringDiv = 0;
    if (++ringDiv >= 4) { ringDiv = 0; updateRing(); }
  }
  if (demoOn) {
    demoAcc += dtMs;
    if (demoAcc > 1800) {
      demoAcc = 0;
      face::setMood((face::Mood)demoIdx, 1700);
      if (++demoIdx >= face::MOOD_COUNT) demoOn = false;
    }
  }
}

void onEvent(const events::Event& e) {
  switch (e.type) {
    case events::EV_SWIPE:
      if (cur <= S_SETTINGS) {
        int next = ((int)cur - e.a + S_SETTINGS + 1) % (S_SETTINGS + 1);   // swipe left (-1) = next
        show((Screen)next);
      }
      break;
    case events::EV_TOUCH_LONG:
      if (cur != S_MENU && cur != S_CALIB) show(S_MENU);
      break;
    case events::EV_SCREEN: show((Screen)e.a); break;
    case events::EV_SETTINGS:
    case events::EV_POWER: applyBrightness(); break;
    default: break;
  }
}

}  // namespace ui
