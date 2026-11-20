// ui_sonar.cpp - radar style distance screen. Forward = up. The sweep is
// cosmetic; the blip shows the real distance at the current head pan angle.
#include "ui_screens.h"
#include "ui_common.h"
#include "sonar.h"
#include "motion.h"
#include "servo.h"
#include <math.h>

namespace ui_sonar {
static lv_obj_t *sweep, *blip, *lDist, *bAvoid;
static float angle = 0;

static void onAvoid(lv_event_t*) {
  motion::setAvoidMode(!motion::avoidMode());
  uic::setButtonText(bAvoid, motion::avoidMode() ? "Avoid: ON" : "Avoid: OFF");
}

lv_obj_t* create(lv_obj_t* parent) {
  lv_obj_t* p = uic::panel(parent);
  for (int r = 60; r <= 200; r += 70) {                       // range rings
    lv_obj_t* c = lv_obj_create(p);
    lv_obj_remove_style_all(c);
    lv_obj_set_size(c, r * 2, r * 2);
    lv_obj_center(c);
    lv_obj_set_style_radius(c, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(c, 1, 0);
    lv_obj_set_style_border_color(c, lv_color_hex(0x105020), 0);
    lv_obj_remove_flag(c, LV_OBJ_FLAG_CLICKABLE);
  }
  sweep = lv_arc_create(p);
  lv_obj_set_size(sweep, 400, 400);
  lv_obj_center(sweep);
  lv_obj_remove_style(sweep, nullptr, LV_PART_KNOB);
  lv_obj_remove_flag(sweep, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_arc_opa(sweep, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_arc_width(sweep, 200, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(sweep, lv_color_hex(0x0C5A20), LV_PART_INDICATOR);
  lv_obj_set_style_arc_opa(sweep, LV_OPA_50, LV_PART_INDICATOR);
  lv_arc_set_bg_angles(sweep, 0, 360);
  lv_arc_set_angles(sweep, 0, 25);
  blip = lv_obj_create(p);
  lv_obj_remove_style_all(blip);
  lv_obj_set_size(blip, 16, 16);
  lv_obj_set_style_radius(blip, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(blip, lv_color_hex(0xFF4040), 0);
  lv_obj_set_style_bg_opa(blip, LV_OPA_COVER, 0);
  lv_obj_add_flag(blip, LV_OBJ_FLAG_HIDDEN);
  lDist = uic::label(p, &lv_font_montserrat_28, 0x40FF70, "-- cm", 330);
  bAvoid = uic::button(p, "Avoid: OFF", 163, 380, 140, 40, onAvoid);
  return p;
}

void update() {
  angle = fmodf(angle + 14, 360);
  lv_arc_set_rotation(sweep, (int)angle);
  int cm = sonar::distanceCm();
  char buf[24];
  if (cm < 0) {
    uic::setText(lDist, "-- cm");
    lv_obj_add_flag(blip, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  snprintf(buf, sizeof(buf), "%d cm", cm);
  uic::setText(lDist, buf);
  float r = fminf((float)cm, 200.f);
  float a = (servo::current(SERVO_HEAD_PAN) - 90.f) * DEG_TO_RAD;
  lv_obj_set_pos(blip, (int)(LCD_W / 2 + r * sinf(a) - 8), (int)(LCD_H / 2 - r * cosf(a) - 8));
  lv_obj_set_style_bg_color(blip, lv_color_hex(cm < 15 ? 0xFF4040 : 0xFFD040), 0);
  lv_obj_remove_flag(blip, LV_OBJ_FLAG_HIDDEN);
}
}  // namespace ui_sonar
