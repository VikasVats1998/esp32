// ui_sonar.cpp - ultrasonic radar (UI kit): a cone that looks forward (up), range arcs, an orange
// blip at the measured distance (turned by the neck angle), big distance, explore-mode switch.
#include "ui_screens.h"
#include "ui_common.h"
#include "sonar.h"
#include "servo.h"
#include "behavior.h"
#include <math.h>

namespace ui_sonar {
static lv_obj_t *root, *lDist, *bExplore;
static int shownCm = -2;
static const float AX = 233, AY = 250, RANGE = 190;   // apex of the cone, radius for 200 cm

static void onExplore(lv_event_t*) {
  behavior::setMode(behavior::mode() == behavior::MODE_EXPLORE ? behavior::MODE_AUTO : behavior::MODE_EXPLORE);
}

static void drawRadar(lv_event_t* e) {
  lv_layer_t* layer = lv_event_get_layer(e);
  // cone: 3 slices from dark teal (edge) to brighter (centre)
  static const uint32_t COL[3] = {0x0B3A40, 0x0E4F57, 0x12666F};
  for (int s = 0; s < 3; s++) {
    float half = (40 - s * 12) * DEG_TO_RAD;
    lv_draw_triangle_dsc_t d;
    lv_draw_triangle_dsc_init(&d);
    d.bg_color = lv_color_hex(COL[s]);
    d.p[0] = {(int32_t)AX, (int32_t)AY};
    d.p[1] = {(int32_t)(AX - RANGE * sinf(half)), (int32_t)(AY - RANGE * cosf(half))};
    d.p[2] = {(int32_t)(AX + RANGE * sinf(half)), (int32_t)(AY - RANGE * cosf(half))};
    lv_draw_triangle(layer, &d);
  }
  lv_draw_arc_dsc_t a;
  lv_draw_arc_dsc_init(&a);
  a.color = lv_color_hex(0x1F5F66);
  a.width = 2;
  a.center = {(int32_t)AX, (int32_t)AY};
  for (int r = 1; r <= 3; r++) {
    a.radius = (uint16_t)(RANGE * r / 3);
    a.start_angle = 230; a.end_angle = 310;
    lv_draw_arc(layer, &a);
  }
  if (shownCm >= 0) {
    float r = fminf(shownCm, 200) / 200.f * RANGE;
    float ang = (servo::current(SERVO_HEAD_PAN) - 90.f) * DEG_TO_RAD;
    lv_draw_rect_dsc_t b;
    lv_draw_rect_dsc_init(&b);
    b.bg_color = lv_color_hex(shownCm < 15 ? uic::C_RED : uic::C_ORANGE);
    b.radius = LV_RADIUS_CIRCLE;
    b.shadow_width = 14; b.shadow_color = b.bg_color; b.shadow_opa = 160;
    int x = (int)(AX + r * sinf(ang)), y = (int)(AY - r * cosf(ang));
    lv_area_t ar = {x - 8, y - 8, x + 8, y + 8};
    lv_draw_rect(layer, &b, &ar);
  }
}

lv_obj_t* create(lv_obj_t* parent) {
  root = uic::panel(parent);
  lv_obj_add_event_cb(root, drawRadar, LV_EVENT_DRAW_MAIN_END, nullptr);
  lDist = uic::label(root, &inter_44, uic::C_TEXT, "--", 262);
  uic::label(root, &inter_14, uic::C_MUTED, "HC-SR04P on the ESP32-CAM", 318);
  uic::chip(root, "< 15 cm \xE2\x86\x92 scared + back up", 342, uic::C_ORANGE, 0x2A1E0C);
  bExplore = uic::button(root, "Explore: off", 163, 380, 140, 34, onExplore, nullptr, 0x2A3038, uic::C_TEXT, &inter_14);
  return root;
}

void update(uint32_t) {
  int cm = sonar::distanceCm();
  if (cm != shownCm) {
    shownCm = cm;
    char buf[24];
    if (cm >= 0) snprintf(buf, sizeof(buf), "%d cm", cm); else strlcpy(buf, "-- cm", sizeof(buf));
    uic::setText(lDist, buf);
    lv_area_t a = {(int32_t)(AX - RANGE - 10), (int32_t)(AY - RANGE - 10), (int32_t)(AX + RANGE + 10), (int32_t)AY + 10};
    lv_obj_invalidate_area(root, &a);
  }
  uic::setButtonText(bExplore, behavior::mode() == behavior::MODE_EXPLORE ? "Explore: ON" : "Explore: off");
}
void onShow() { shownCm = -2; }
void onHide() {}
}  // namespace ui_sonar
