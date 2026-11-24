// ui_power.cpp - power menu (UI kit, hold the face 3 s): Sleep / Restart / Power off.
#include "ui_screens.h"
#include "ui_common.h"
#include "ui.h"
#include "power.h"

namespace ui_power {
static uint32_t restartAt = 0;

static void onSleep(lv_event_t*) { ui::show(ui::S_FACE); power::sleepNow(); }
static void onRestart(lv_event_t*) { ui::toast("Restarting..."); restartAt = millis() + 600; }
static void onOff(lv_event_t*) { ui::show(ui::S_GOODNIGHT); }

lv_obj_t* create(lv_obj_t* parent) {
  lv_obj_t* p = uic::panel(parent);
  uic::title(p, "Power", 92);
  uic::circleButton(p, LV_SYMBOL_POWER, 150, 196, 86, uic::C_PURPLE, onSleep, nullptr, &inter_28);
  uic::circleButton(p, LV_SYMBOL_POWER, 316, 196, 86, uic::C_YELLOW, onRestart, nullptr, &inter_28);
  uic::circleButton(p, LV_SYMBOL_POWER, 233, 282, 86, uic::C_RED, onOff, nullptr, &inter_28);
  uic::label(p, &inter_16, uic::C_TEXT, "Sleep", 248, LV_ALIGN_TOP_LEFT, 128);
  uic::label(p, &inter_16, uic::C_TEXT, "Restart", 248, LV_ALIGN_TOP_LEFT, 287);
  uic::label(p, &inter_16, uic::C_TEXT, "Power off", 334);
  uic::label(p, &inter_14, uic::C_DIM, "Hold the screen 3 s to open", 392);
  return p;
}
void update(uint32_t) { if (restartAt && millis() > restartAt) ESP.restart(); }
void onShow() { restartAt = 0; }
void onHide() {}
}  // namespace ui_power
