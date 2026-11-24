// ui_calib.cpp - servo calibration (UI kit): a body diagram with the 5 joints. Joint by joint
// (neck, left hip, right hip, left foot, right foot): the servo goes to its centre (90 deg + trim),
// "-" / "+" move it 1 deg until the part sits straight, "Save" stores it and goes to the next joint.
// "reverse direction" flips a servo that turns the wrong way. Saved in NVS.
#include "ui_screens.h"
#include "ui_common.h"
#include "ui.h"
#include "storage.h"
#include "servo.h"
#include "events.h"

namespace ui_calib {
struct Joint { uint8_t ch; const char* name; int x, y; int lx, ly; bool labelLeft; };
static const Joint J[SERVO_COUNT] = {
  {SERVO_HEAD_PAN, "NECK", 233, 170, 262, 160, false},
  {SERVO_LEFT_HIP, "L HIP", 205, 240, 70, 230, true},
  {SERVO_RIGHT_HIP, "R HIP", 261, 240, 286, 230, false},
  {SERVO_LEFT_ANKLE, "L FOOT", 195, 298, 60, 288, true},
  {SERVO_RIGHT_ANKLE, "R FOOT", 271, 298, 296, 288, false},
};
static lv_obj_t *dot[SERVO_COUNT], *lbl[SERVO_COUNT], *progress;
static int sel = 0;
static bool done[SERVO_COUNT];
static bool setupMode = false, finishedFlag = false;

void setSetupMode(bool on) { setupMode = on; finishedFlag = false; }
bool finished() { return finishedFlag; }

static void centre() { servo::setTarget(J[sel].ch, 90, 60); servo::hold(J[sel].ch); }

static void refresh() {
  Settings& s = storage::get();
  for (int i = 0; i < SERVO_COUNT; i++) {
    char b[32];
    uint32_t c;
    if (i == sel) { snprintf(b, sizeof(b), "%s %d\xC2\xB0", J[i].name, 90 + s.servo[J[i].ch].trim); c = uic::C_YELLOW; }
    else if (done[i]) { snprintf(b, sizeof(b), "%s " LV_SYMBOL_OK, J[i].name); c = uic::C_GREEN; }
    else if (i == sel + 1) { snprintf(b, sizeof(b), "%s next", J[i].name); c = uic::C_MUTED; }
    else { strlcpy(b, J[i].name, sizeof(b)); c = uic::C_DIM; }
    uic::setText(lbl[i], b);
    uic::setColor(lbl[i], c);
    lv_obj_set_style_bg_color(dot[i], lv_color_hex(i == sel ? uic::C_YELLOW : done[i] ? uic::C_GREEN : 0x000000), 0);
    lv_obj_set_style_border_color(dot[i], lv_color_hex(i == sel ? uic::C_YELLOW : done[i] ? uic::C_GREEN : uic::C_MUTED), 0);
  }
  int n = 0;
  for (bool d : done) n += d;
  uic::setDots(progress, n + 1);
}

static void onTrim(lv_event_t* e) {
  ServoCfg& c = storage::get().servo[J[sel].ch];
  c.trim = constrain(c.trim + (int)(intptr_t)lv_event_get_user_data(e), -45, 45);
  servo::hold(J[sel].ch);                    // output again with the new trim
  refresh();
}
static void onReverse(lv_event_t*) {
  ServoCfg& c = storage::get().servo[J[sel].ch];
  c.invert = !c.invert;
  centre();
  ui::toast(c.invert ? "Direction reversed" : "Direction normal");
}
static void onSave(lv_event_t*) {
  done[sel] = true;
  servo::setTarget(J[sel].ch, 90, 60);
  if (sel < SERVO_COUNT - 1) { sel++; centre(); refresh(); return; }
  Settings& s = storage::get();
  s.servoCalibrated = true;
  storage::save();
  events::publish(events::EV_SETTINGS);
  finishedFlag = true;
  if (setupMode) ui::show(ui::S_SETUP); else ui::back();
}

static lv_obj_t* outline(lv_obj_t* p, int x, int y, int w, int h, uint32_t c, int r) {
  lv_obj_t* o = uic::plain(p);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, w, h);
  lv_obj_set_style_radius(o, r, 0);
  lv_obj_set_style_border_width(o, 4, 0);
  lv_obj_set_style_border_color(o, lv_color_hex(c), 0);
  return o;
}

lv_obj_t* create(lv_obj_t* parent) {
  lv_obj_t* p = uic::panel(parent);
  uic::title(p, "Servo calibration", 52);
  outline(p, 178, 92, 110, 68, uic::C_CYAN, 14);           // head
  outline(p, 188, 180, 90, 46, uic::C_GREY, 10);           // body
  static lv_point_precise_t legs[2][2] = {{{205, 240}, {195, 298}}, {{261, 240}, {271, 298}}};
  for (auto& l : legs) {
    lv_obj_t* ln = lv_line_create(p);
    lv_line_set_points(ln, l, 2);
    lv_obj_set_style_line_width(ln, 4, 0);
    lv_obj_set_style_line_color(ln, lv_color_hex(uic::C_GREY), 0);
  }
  for (int i = 0; i < SERVO_COUNT; i++) {
    dot[i] = uic::plain(p);
    lv_obj_set_size(dot[i], 22, 22);
    lv_obj_set_pos(dot[i], J[i].x - 11, J[i].y - 11);
    lv_obj_set_style_radius(dot[i], LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(dot[i], LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(dot[i], 3, 0);
    lbl[i] = uic::label(p, &inter_14, uic::C_DIM, "", J[i].ly, LV_ALIGN_TOP_LEFT, J[i].lx);
    lv_obj_set_width(lbl[i], 110);
    lv_obj_set_style_text_align(lbl[i], J[i].labelLeft ? LV_TEXT_ALIGN_RIGHT : LV_TEXT_ALIGN_LEFT, 0);
  }
  uic::button(p, "\xE2\x80\x93", 110, 326, 64, 44, onTrim, (void*)-1, uic::C_CARD_HI, uic::C_TEXT, &inter_28);
  lv_obj_t* b = uic::button(p, "+", 292, 326, 64, 44, onTrim, (void*)1, uic::C_CARD_HI, uic::C_TEXT, &inter_28);
  (void)b;
  uic::button(p, "Save", 183, 326, 100, 44, onSave, nullptr, uic::C_CYAN, 0x000000, &inter_16);
  uic::button(p, "reverse direction", 158, 376, 150, 28, onReverse, nullptr, 0x000000, uic::C_MUTED, &inter_14);
  progress = uic::dots(p, SERVO_COUNT, 410);
  return p;
}
void update(uint32_t) {}
void onShow() { sel = 0; for (bool& d : done) d = false; finishedFlag = false; refresh(); centre(); }
void onHide() { servo::setTarget(J[sel].ch, 90, 60); setupMode = false; }
}  // namespace ui_calib
