// ui_menu.cpp - long-press menu: pick a gait, mood demo, avoid mode, calibration...
#include "ui_screens.h"
#include "ui_common.h"
#include "ui.h"
#include "events.h"
#include "motion.h"
#include "face.h"
#include "net.h"
#include "storage.h"
#include "touch.h"

namespace ui_menu {
static lv_obj_t* roller;
// Order must match the option string below.
static const char* OPTIONS =
    "Home\nWalk\nBack up\nTurn left\nTurn right\nBend\nShake leg\nMoonwalk\nDance\nJump\nWiggle\n"
    "Avoid mode on\nAvoid mode off\nMood demo\nCalibrate servos\nWi-Fi reset\nReboot";

static void onOk(lv_event_t*) {
  uint32_t i = lv_roller_get_selected(roller);
  if (i <= 10) {
    ui::show(ui::S_FACE);
    events::publish(events::EV_MOTION_CMD, i, 4);
  } else switch (i) {
    case 11: ui::show(ui::S_FACE); motion::setAvoidMode(true); break;
    case 12: motion::setAvoidMode(false); break;
    case 13: ui::show(ui::S_FACE); ui::startMoodDemo(); break;
    case 14: ui::show(ui::S_CALIB); break;
    case 15: storage::clearWifi(); net::restartWifi(); ui::show(ui::S_FACE); break;
    case 16: ESP.restart(); break;
  }
}
static void onClose(lv_event_t*) { ui::show(ui::S_FACE); }

lv_obj_t* create(lv_obj_t* parent) {
  lv_obj_t* p = uic::panel(parent);
  uic::label(p, &lv_font_montserrat_20, 0xFFFFFF, "Menu", 40);
  roller = lv_roller_create(p);
  lv_roller_set_options(roller, OPTIONS, LV_ROLLER_MODE_NORMAL);
  lv_roller_set_visible_row_count(roller, 5);
  lv_obj_set_width(roller, 240);
  lv_obj_align(roller, LV_ALIGN_TOP_MID, 0, 85);
  lv_obj_set_style_bg_color(roller, lv_color_hex(0x182030), 0);
  lv_obj_set_style_text_color(roller, lv_color_white(), 0);
  lv_obj_set_style_bg_color(roller, lv_color_hex(0x2A5AA0), LV_PART_SELECTED);
  lv_obj_set_style_border_width(roller, 0, 0);
  uic::button(p, "OK", 98, 335, 120, 46, onOk, nullptr, 0x2A6A3A);
  uic::button(p, "Close", 248, 335, 120, 46, onClose);
  return p;
}
void update() {}
void onShow() { touch::setGestures(false); }
}  // namespace ui_menu
