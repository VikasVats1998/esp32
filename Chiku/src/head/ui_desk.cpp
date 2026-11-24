// ui_desk.cpp - control the standing desk from Chiku's screen (Settings -> Standing desk).
// UP / DOWN work while held (dead-man: the desk stops when you let go), STOP, memory positions 1-4.
#include "ui_screens.h"
#include "ui_common.h"
#include "ui.h"
#include "desk.h"

namespace ui_desk {
static lv_obj_t *lHeight, *lInfo;
static volatile int holdDir = 0;

static void onHold(lv_event_t* e) { holdDir = (int)(intptr_t)lv_event_get_user_data(e); desk::moveHold(holdDir); }
static void onRelease(lv_event_t*) {
  if (holdDir) { holdDir = 0; desk::moveHold(0); desk::command("stop"); }
}
static void onStop(lv_event_t*) { holdDir = 0; desk::moveHold(0); desk::command("stop"); }
static void onPreset(lv_event_t* e) { desk::command("preset", (int)(intptr_t)lv_event_get_user_data(e)); }
static void onBack(lv_event_t*) { holdDir = 0; ui::back(); }

// Keeps sending "move" while a button is held (every 200 ms).
static void holdTimer(lv_timer_t*) {
  if (holdDir && ui::current() == ui::S_DESK) desk::moveHold(holdDir);
  else if (holdDir) holdDir = 0;
}

static lv_obj_t* holdButton(lv_obj_t* p, const char* text, int x, int y, int w, int h, int dir) {
  lv_obj_t* b = uic::button(p, text, x, y, w, h, nullptr, nullptr, uic::C_CARD_HI);
  lv_obj_add_event_cb(b, onHold, LV_EVENT_PRESSED, (void*)(intptr_t)dir);
  lv_obj_add_event_cb(b, onRelease, LV_EVENT_RELEASED, nullptr);
  lv_obj_add_event_cb(b, onRelease, LV_EVENT_PRESS_LOST, nullptr);
  return b;
}

lv_obj_t* create(lv_obj_t* parent) {
  lv_obj_t* p = uic::panel(parent);
  uic::title(p, "Standing desk", 52);
  lHeight = uic::label(p, &inter_44, uic::C_TEXT, "--.-", 80);
  lInfo = uic::label(p, &inter_14, uic::C_MUTED, "", 134);
  holdButton(p, LV_SYMBOL_UP " UP", 163, 150, 140, 54, 1);
  holdButton(p, LV_SYMBOL_DOWN " DOWN", 163, 212, 140, 54, -1);
  uic::button(p, "STOP", 313, 180, 80, 54, onStop, nullptr, uic::C_RED);
  for (int i = 1; i <= 4; i++) uic::button(p, String(i).c_str(), 63 + (i - 1) * 85, 282, 74, 46, onPreset, (void*)(intptr_t)i);
  uic::button(p, "Back", 173, 345, 120, 44, onBack);
  lv_timer_create(holdTimer, 200, nullptr);
  return p;
}
void onShow() {}
void onHide() { onStop(nullptr); }

void update(uint32_t) {
  desk::State s = desk::state();
  char buf[64];
  if (!s.paired) {
    uic::setText(lHeight, "--.-");
    uic::setText(lInfo, "not paired: use the app (Control, Desk)");
    return;
  }
  if (s.hv) snprintf(buf, sizeof(buf), "%.1f", s.h); else snprintf(buf, sizeof(buf), "--.-");
  uic::setText(lHeight, buf);
  if (!s.ok) snprintf(buf, sizeof(buf), "no link: %s", s.err);
  else snprintf(buf, sizeof(buf), "%s%s%s  (%s)", s.dir > 0 ? LV_SYMBOL_UP " " : s.dir < 0 ? LV_SYMBOL_DOWN " " : "", s.moving || s.travel ? "moving" : "still", s.armed ? "" : "  LOCKED", s.src);
  uic::setText(lInfo, buf);
}
}  // namespace ui_desk
