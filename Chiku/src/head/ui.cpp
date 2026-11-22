// ui.cpp - see ui.h
#include "ui.h"
#include "config.h"
#include "ui_screens.h"
#include "face.h"
#include "display.h"
#include "logbuf.h"
#include "imu.h"
#include "touch.h"
#include "power.h"
#include "storage.h"
#include "alarm_timer.h"

namespace ui {

static lv_obj_t* screens[S_COUNT];
static Screen cur = S_FACE;
// Swipe carousel (S_MENU, S_CALIB, S_DESK are reached from the menu only).
static const Screen ORDER[] = {S_FACE, S_CLOCK, S_WEATHER, S_TIMER, S_SONAR, S_CAMERA, S_SETTINGS,
                               S_LIGHT, S_DEVICES, S_FAN, S_MOTION, S_SYSTEM};
static const int ORDER_N = sizeof(ORDER) / sizeof(ORDER[0]);
static lv_obj_t* testObj = nullptr;       // full-screen colour for display tests
static volatile int reqPattern = 0;
static int shownPattern = 0;
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
  touch::setGestures(s != S_MENU && s != S_CALIB && s != S_DESK);
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
  screens[S_DESK] = ui_desk::create(root);
  screens[S_LIGHT] = ui_light::create(root);
  screens[S_DEVICES] = ui_devices::create(root);
  screens[S_FAN] = ui_fan::create(root);
  screens[S_MOTION] = ui_motion::create(root);
  screens[S_SYSTEM] = ui_system::create(root);

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
  testObj = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(testObj);
  lv_obj_set_size(testObj, LCD_W, LCD_H);
  lv_obj_set_style_bg_opa(testObj, LV_OPA_COVER, 0);
  lv_obj_remove_flag(testObj, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(testObj, LV_OBJ_FLAG_HIDDEN);
  show(S_FACE);
}

void setTestPattern(int p) { reqPattern = p; }

static void applyPattern() {
  int p = reqPattern;
  if (p == shownPattern) return;
  shownPattern = p;
  if (p == 0) { lv_obj_add_flag(testObj, LV_OBJ_FLAG_HIDDEN); return; }
  static const uint32_t C[] = {0, 0xFF0000, 0x00FF00, 0x0000FF, 0xFFFFFF, 0x000000, 0x000000};
  lv_obj_set_style_bg_color(testObj, lv_color_hex(C[p > 6 ? 5 : p]), 0);
  if (p == 6) {                                       // grey ramp: look for banding
    lv_obj_set_style_bg_grad_color(testObj, lv_color_white(), 0);
    lv_obj_set_style_bg_grad_dir(testObj, LV_GRAD_DIR_HOR, 0);
  } else lv_obj_set_style_bg_grad_dir(testObj, LV_GRAD_DIR_NONE, 0);
  lv_obj_remove_flag(testObj, LV_OBJ_FLAG_HIDDEN);
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

// Keep the picture "ground down": when the board is held upright, pick the 90-degree step whose
// bottom points at the ground. The switch happens at ~45 deg (with a margin so it never flutters);
// lying flat leaves the current orientation alone.
static void autoRotate() {
  static int cand = -1;
  static uint32_t candSince = 0;
  float ax, ay, az;
  imu::getAccel(ax, ay, az);
  if (fabsf(az) > 0.8f) return;                              // flat on a table
  float mag = max(fabsf(ax), fabsf(ay));
  if (mag < 0.5f) return;
  int want;
  if (fabsf(ax) > fabsf(ay) * 1.25f) want = ax < 0 ? 0 : 180;
  else if (fabsf(ay) > fabsf(ax) * 1.25f) want = ay > 0 ? 270 : 90;
  else return;                                               // near the 45 deg boundary: keep as is
  if (want == display::rotation()) { cand = -1; return; }
  if (want != cand) { cand = want; candSince = millis(); return; }
  if (millis() - candSince > 350) {                          // held steady
    LOGI("ui", "rotate %d (ax %.2f ay %.2f)", want, ax, ay);
    display::setRotation(want);
    cand = -1;
  }
}

void update(uint32_t dtMs) {
  applyPattern();
  if (cur == S_FACE) face::update(dtMs);
  if (cur == S_MOTION) ui_motion::update();                 // bubble level: every frame
  slowAcc += dtMs;
  if (slowAcc >= 100) autoRotate();
  if (slowAcc >= 250) {
    slowAcc = 0;
    switch (cur) {
      case S_CLOCK: ui_clock::update(); break;
      case S_WEATHER: ui_weather::update(); break;
      case S_TIMER: ui_timer::update(); break;
      case S_SONAR: ui_sonar::update(); break;
      case S_CAMERA: ui_camera::update(); break;
      case S_SETTINGS: ui_settings::update(); break;
      case S_DESK: ui_desk::update(); break;
      case S_LIGHT: ui_light::update(); break;
      case S_DEVICES: ui_devices::update(); break;
      case S_FAN: ui_fan::update(); break;
      case S_SYSTEM: ui_system::update(); break;
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
      for (int i = 0; i < ORDER_N; i++) {
        if (ORDER[i] != cur) continue;
        show(ORDER[(i - e.a + ORDER_N) % ORDER_N]);          // swipe left (-1) = next
        break;
      }
      break;
    case events::EV_TOUCH_LONG:
      if (cur != S_FACE && cur != S_MENU && cur != S_CALIB && cur != S_DESK) show(S_MENU);   // on the face a long press = love
      break;
    case events::EV_SCREEN: show((Screen)e.a); break;
    case events::EV_SETTINGS:
    case events::EV_POWER: applyBrightness(); break;
    default: break;
  }
}

}  // namespace ui
