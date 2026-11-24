// ui_moves.cpp - move library (menu -> Moves): every gait and emotion move as a chip, plus explore
// mode (walk around and avoid obstacles). Tap a move: Chiku goes back to its face and plays it.
#include "ui_screens.h"
#include "ui_common.h"
#include "ui.h"
#include "events.h"
#include "motion.h"
#include "behavior.h"

namespace ui_moves {
static lv_obj_t* bExplore;

static void onMove(lv_event_t* e) {
  ui::show(ui::S_FACE);
  events::publish(events::EV_MOTION_CMD, (int)(intptr_t)lv_event_get_user_data(e), 3);
}
static void onExplore(lv_event_t*) {
  bool on = behavior::mode() != behavior::MODE_EXPLORE;
  behavior::setMode(on ? behavior::MODE_EXPLORE : behavior::MODE_AUTO);
  if (on) ui::show(ui::S_FACE);
}

lv_obj_t* create(lv_obj_t* parent) {
  lv_obj_t* p = uic::panel(parent);
  uic::title(p, "Moves", 58);
  lv_obj_t* l = uic::list(p, 96, 300, 330);
  lv_obj_set_flex_flow(l, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_flex_align(l, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_column(l, 8, 0);
  bExplore = uic::button(l, "Explore: off", 0, 0, 320, 48, onExplore, nullptr, uic::C_PURPLE, 0x000000);
  for (int g = 1; g < motion::G_COUNT; g++)                // 0 = home
    uic::button(l, motion::gaitName((motion::Gait)g), 0, 0, 104, 44, onMove, (void*)(intptr_t)g);
  return p;
}
void update(uint32_t) {
  uic::setButtonText(bExplore, behavior::mode() == behavior::MODE_EXPLORE ? "Explore: ON" : "Explore: off");
}
void onShow() {}
void onHide() {}
}  // namespace ui_moves
