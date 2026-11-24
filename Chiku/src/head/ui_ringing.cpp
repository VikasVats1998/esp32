// ui_ringing.cpp - alarm / timer ringing (UI kit): yellow ring, mini eyes, the time, Snooze 5' / Stop.
// Touching a head pad or shaking also stops it (behavior.cpp).
#include "ui_screens.h"
#include "ui_common.h"
#include "alarm_timer.h"
#include "rtc_time.h"
#include <math.h>

namespace ui_ringing {
static lv_obj_t *ring, *lTime, *lSub;
static uic::MiniEyes eyes;
static float ph = 0;

static void onSnooze(lv_event_t*) { alarm_timer::snooze(5); }
static void onStop(lv_event_t*) { alarm_timer::stop(); }

lv_obj_t* create(lv_obj_t* parent) {
  lv_obj_t* p = uic::panel(parent);
  ring = uic::ring(p, 430, 10, uic::C_YELLOW, 0x2A2410);
  uic::setRing(ring, 1000);
  eyes.create(p, 100, 34, 34, 64, uic::C_YELLOW);
  lTime = uic::label(p, &inter_72, uic::C_TEXT, "--:--", 150);
  lSub = uic::label(p, &inter_16, uic::C_MUTED, "", 246);
  uic::button(p, "Snooze 5'", 103, 290, 124, 50, onSnooze, nullptr, 0x2A3038);
  uic::button(p, "Stop", 241, 290, 124, 50, onStop, nullptr, uic::C_YELLOW, 0x000000, &inter_20);
  uic::label(p, &inter_14, uic::C_MUTED, "Touch my head or shake me to stop", 362);
  return p;
}

void update(uint32_t dt) {
  eyes.update(dt);
  ph += dt / 1000.f;
  lv_obj_set_style_arc_opa(ring, (lv_opa_t)(150 + 105 * sinf(ph * 6)), LV_PART_INDICATOR);   // pulse
  char buf[32];
  struct tm t;
  if (rtc_time::localTime(t)) { snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min); uic::setText(lTime, buf); }
  int id = alarm_timer::ringingId();
  uic::setText(lSub, id >= 100 ? "Time is up!" : "Wake up! \xC2\xB7 Alarm");
}
void onShow() { ph = 0; }
void onHide() {}
}  // namespace ui_ringing
