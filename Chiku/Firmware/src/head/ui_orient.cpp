// ui_orient.cpp - screen orientation. The round panel can be mounted at any angle, so the picture
// (and touch) can be turned by any number of degrees, not only 90 deg steps.
//   Auto: Chiku stands upright, the IMU measures where "down" is in the panel plane -> exact angle.
//   - / +: fine-tune 1 deg (hold: 5 deg) until the arrow points straight up and the line is level.
//   Save: stored in NVS, used from the next boot on.
// The IMU axis mapping is in config.h (IMU_SCREEN_X / IMU_SCREEN_Y).
#include "ui_screens.h"
#include "ui_common.h"
#include "ui.h"
#include "storage.h"
#include "display.h"
#include "imu.h"
#include <math.h>

namespace ui_orient {
static lv_obj_t *lVal, *lLevel, *sdots;
static int val = 0;
static bool setupMode = false, finishedFlag = false;
static uint32_t acc = 0;

void setSetupMode(bool on) { setupMode = on; finishedFlag = false; }
bool finished() { return finishedFlag; }

static int norm(int d) { return ((d % 360) + 360) % 360; }

// Angle the picture must be turned so "up" on screen is real up; -1 if it cannot be measured.
static int measured() {
  if (!imu::online()) return -1;
  float ax, ay, az;
  imu::getAccel(ax, ay, az);
  float gx = IMU_SCREEN_X(ax, ay), gy = IMU_SCREEN_Y(ax, ay);
  if (sqrtf(gx * gx + gy * gy) < 0.5f) return -1;        // lying flat: no "down" in the panel plane
  return norm((int)lroundf(-atan2f(gx, gy) * RAD_TO_DEG));
}

static void apply(int v) {
  val = norm(v);
  display::setRotation(val);
  char b[16];
  snprintf(b, sizeof(b), "%d\xC2\xB0", val);
  uic::setText(lVal, b);
}
static void onStep(lv_event_t* e) {
  int step = (int)(intptr_t)lv_event_get_user_data(e);
  apply(val + (lv_event_get_code(e) == LV_EVENT_LONG_PRESSED_REPEAT ? step * 5 : step));
}
static void onAuto(lv_event_t*) {
  int m = measured();
  if (m < 0) ui::toast(imu::online() ? "Stand me upright first" : "IMU offline: turn by hand");
  else apply(m);
}
static void onSave(lv_event_t*) {
  storage::get().screenRot = val;
  storage::save();
  finishedFlag = true;
  if (setupMode) ui::show(ui::S_SETUP); else ui::back();
}

lv_obj_t* create(lv_obj_t* parent) {
  lv_obj_t* p = uic::panel(parent);
  uic::title(p, "Screen orientation", 50);
  uic::label(p, &inter_44, uic::C_CYAN, "\xE2\x86\x91", 80);       // up arrow
  uic::label(p, &inter_14, uic::C_MUTED, "UP", 136);
  lv_obj_t* line = uic::plain(p);
  lv_obj_set_size(line, 300, 4);
  lv_obj_set_style_radius(line, 2, 0);
  lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(line, lv_color_hex(uic::C_CYAN), 0);
  lv_obj_align(line, LV_ALIGN_TOP_MID, 0, 168);
  lVal = uic::label(p, &inter_44, uic::C_TEXT, "0\xC2\xB0", 184);
  lLevel = uic::label(p, &inter_14, uic::C_MUTED, "", 246);
  for (int s : {-1, 1}) {
    lv_obj_t* b = uic::button(p, s < 0 ? "\xE2\x80\x93" : "+", s < 0 ? 98 : 304, 280, 64, 46, nullptr, nullptr,
                              uic::C_CARD_HI, uic::C_TEXT, &inter_28);
    lv_obj_add_event_cb(b, onStep, LV_EVENT_SHORT_CLICKED, (void*)(intptr_t)s);
    lv_obj_add_event_cb(b, onStep, LV_EVENT_LONG_PRESSED_REPEAT, (void*)(intptr_t)s);
  }
  uic::button(p, "Auto", 175, 280, 116, 46, onAuto, nullptr, 0x0E3A40, uic::C_CYAN, &inter_16);
  uic::button(p, "Save", 173, 342, 120, 46, onSave, nullptr, uic::C_CYAN, 0x000000, &inter_16);
  sdots = uic::dots(p, 6, 404);
  uic::setDots(sdots, 3);
  return p;
}

void update(uint32_t dt) {
  acc += dt;
  if (acc < 250) return;
  acc = 0;
  int m = measured();
  char b[48];
  if (m < 0) strlcpy(b, imu::online() ? "Stand me upright to measure" : "No IMU: turn until it looks right", sizeof(b));
  else {
    int diff = norm(m - val + 180) - 180;
    if (abs(diff) <= 1) snprintf(b, sizeof(b), "Level " LV_SYMBOL_OK);
    else snprintf(b, sizeof(b), "%d\xC2\xB0 off \xC2\xB7 tap Auto", diff);
    uic::setColor(lLevel, abs(diff) <= 1 ? uic::C_GREEN : uic::C_MUTED);
  }
  uic::setText(lLevel, b);
}
void onShow() { finishedFlag = false; uic::show(sdots, setupMode); apply(display::rotation()); acc = 250; }
void onHide() {
  if (!finishedFlag) display::setRotation(storage::get().screenRot);   // left without saving
  setupMode = false;
}
}  // namespace ui_orient
