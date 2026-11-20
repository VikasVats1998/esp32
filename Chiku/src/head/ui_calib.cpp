// ui_calib.cpp - servo calibration: choose a servo, move it to its centre
// (90 deg + trim) and adjust the trim until the part sits straight.
// Saved in NVS. Limits/invert can also be set on the web page.
#include "ui_screens.h"
#include "ui_common.h"
#include "ui.h"
#include "storage.h"
#include "servo.h"
#include "touch.h"
#include "events.h"

namespace ui_calib {
static lv_obj_t *lName, *lTrim, *bInv;
static int sel = 0;
static const char* NAMES[SERVO_COUNT] = {"Left hip", "Right hip", "Left ankle", "Right ankle", "Head pan"};

static void centre() { servo::setTarget(sel, 90, 60); servo::hold(sel); }

static void refresh() {
  ServoCfg& c = storage::get().servo[sel];
  char buf[48];
  snprintf(buf, sizeof(buf), "%d: %s", sel, NAMES[sel]);
  uic::setText(lName, buf);
  snprintf(buf, sizeof(buf), "Trim %+d deg", c.trim);
  uic::setText(lTrim, buf);
  uic::setButtonText(bInv, c.invert ? "Invert: ON" : "Invert: OFF");
}
static void onSel(lv_event_t* e) {
  sel = (sel + SERVO_COUNT + (int)(intptr_t)lv_event_get_user_data(e)) % SERVO_COUNT;
  centre(); refresh();
}
static void onTrim(lv_event_t* e) {
  ServoCfg& c = storage::get().servo[sel];
  c.trim = constrain(c.trim + (int)(intptr_t)lv_event_get_user_data(e), -45, 45);
  servo::hold(sel);                      // output again with the new trim
  refresh();
}
static void onInv(lv_event_t*) {
  ServoCfg& c = storage::get().servo[sel];
  c.invert = !c.invert;
  centre(); refresh();
}
static void onSave(lv_event_t*) {
  storage::get().servoCalibrated = true;
  storage::save();
  events::publish(events::EV_SETTINGS);
  ui::show(ui::S_FACE);
}

lv_obj_t* create(lv_obj_t* parent) {
  lv_obj_t* p = uic::panel(parent);
  uic::label(p, &lv_font_montserrat_14, 0x808080, "SERVO CALIBRATION", 50);
  lName = uic::label(p, &lv_font_montserrat_28, 0xFFFFFF, "", 80);
  uic::button(p, "<", 63, 130, 80, 46, onSel, (void*)-1);
  uic::button(p, ">", 323, 130, 80, 46, onSel, (void*)1);
  lTrim = uic::label(p, &lv_font_montserrat_28, 0xFFD060, "", 205);
  uic::button(p, "-", 100, 250, 80, 46, onTrim, (void*)-1);
  uic::button(p, "+", 286, 250, 80, 46, onTrim, (void*)1);
  bInv = uic::button(p, "Invert", 163, 305, 140, 40, onInv);
  uic::button(p, "Save", 163, 355, 140, 44, onSave, nullptr, 0x2A6A3A);
  return p;
}
void update() {}
void onShow() { touch::setGestures(false); refresh(); centre(); }
void onHide() { touch::setGestures(true); servo::setTarget(sel, 90, 60); }
}  // namespace ui_calib
