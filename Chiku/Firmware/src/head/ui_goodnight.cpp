// ui_goodnight.cpp - power off (UI kit): closed purple eyes, "Good night!", servos go home and are
// released, then power::shutdown(): the AXP2101 switches the board off (on USB: deep sleep, BOOT wakes).
#include "ui_screens.h"
#include "ui_common.h"
#include "motion.h"
#include "servo.h"
#include "audio.h"
#include "power.h"
#include "display.h"

namespace ui_goodnight {
static uint32_t t = 0;
static uint8_t step = 0;

lv_obj_t* create(lv_obj_t* parent) {
  lv_obj_t* p = uic::panel(parent);
  for (int i = 0; i < 2; i++) {                      // closed eyes: lower half arcs
    lv_obj_t* a = lv_arc_create(p);
    lv_obj_set_size(a, 100, 100);
    lv_obj_set_pos(a, i ? 267 : 99, 128);
    lv_obj_remove_style(a, nullptr, LV_PART_KNOB);
    lv_obj_remove_flag(a, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_opa(a, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_arc_width(a, 22, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(a, false, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(a, lv_color_hex(0xC77DFF), LV_PART_INDICATOR);
    lv_arc_set_bg_angles(a, 0, 360);
    lv_arc_set_angles(a, 20, 160);
  }
  uic::label(p, &inter_28, uic::C_TEXT, "Good night!", 252);
  uic::label(p, &inter_14, uic::C_MUTED, "Shutting down...", 296);
  return p;
}

void update(uint32_t dt) {
  t += dt;
  if (step == 0 && t > 1500) { step = 1; servo::releaseAll(); }
  if (step == 1 && t > 2800) { step = 2; power::shutdown(); }
}
void onShow() { t = 0; step = 0; motion::stop(); audio::babble(audio::BAB_SAD, 2); power::keepAwake(true); }
void onHide() { power::keepAwake(false); }
}  // namespace ui_goodnight
