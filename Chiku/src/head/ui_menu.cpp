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
// Roller text = every gait name (index = gait id) followed by the extra actions.
static char options[640];
static const char* EXTRA[] = {"Avoid mode on", "Avoid mode off", "Mood demo", "Calibrate servos", "Wi-Fi reset", "Reboot"};

static void onOk(lv_event_t*) {
  uint32_t i = lv_roller_get_selected(roller);
  if (i < motion::G_COUNT) {
    ui::show(ui::S_FACE);
    events::publish(events::EV_MOTION_CMD, i, 4);
    return;
  }
  switch (i - motion::G_COUNT) {
    case 0: ui::show(ui::S_FACE); motion::setAvoidMode(true); break;
    case 1: motion::setAvoidMode(false); break;
    case 2: ui::show(ui::S_FACE); ui::startMoodDemo(); break;
    case 3: ui::show(ui::S_CALIB); break;
    case 4: storage::clearWifi(); net::restartWifi(); ui::show(ui::S_FACE); break;
    case 5: ESP.restart(); break;
  }
}
static void onClose(lv_event_t*) { ui::show(ui::S_FACE); }

lv_obj_t* create(lv_obj_t* parent) {
  lv_obj_t* p = uic::panel(parent);
  uic::label(p, &lv_font_montserrat_20, 0xFFFFFF, "Menu", 40);
  roller = lv_roller_create(p);
  options[0] = 0;
  for (int g = 0; g < motion::G_COUNT; g++) { strlcat(options, motion::gaitName((motion::Gait)g), sizeof(options)); strlcat(options, "\n", sizeof(options)); }
  for (int k = 0; k < 6; k++) { strlcat(options, EXTRA[k], sizeof(options)); if (k < 5) strlcat(options, "\n", sizeof(options)); }
  lv_roller_set_options(roller, options, LV_ROLLER_MODE_NORMAL);
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
