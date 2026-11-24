// ui_clock.cpp - clock (UI kit): tick ring that fills with the seconds, mini eyes that stay alive,
// big time, date, next alarm.
#include "ui_screens.h"
#include "ui_common.h"
#include "rtc_time.h"
#include "alarm_timer.h"

namespace ui_clock {
static lv_obj_t *root, *lTime, *lDate, *lNext;
static uic::MiniEyes eyes;
static int shownSec = -1;

// 60 ticks around the edge; the ones up to the current second are cyan.
static void drawTicks(lv_event_t* e) {
  lv_layer_t* layer = lv_event_get_layer(e);
  lv_draw_line_dsc_t d;
  lv_draw_line_dsc_init(&d);
  d.round_start = d.round_end = 1;
  for (int i = 0; i < 60; i++) {
    bool major = i % 5 == 0;
    float a = (i * 6 - 90) * DEG_TO_RAD, r1 = major ? 196 : 204, r2 = 214;
    d.width = major ? 4 : 2;
    d.color = lv_color_hex(shownSec >= 0 && i <= shownSec ? uic::C_CYAN : (major ? 0x4B5563 : 0x262C33));
    d.p1 = {(int32_t)(233 + r1 * cosf(a)), (int32_t)(233 + r1 * sinf(a))};
    d.p2 = {(int32_t)(233 + r2 * cosf(a)), (int32_t)(233 + r2 * sinf(a))};
    lv_draw_line(layer, &d);
  }
}

lv_obj_t* create(lv_obj_t* parent) {
  root = uic::panel(parent);
  lv_obj_add_event_cb(root, drawTicks, LV_EVENT_DRAW_MAIN_END, nullptr);
  eyes.create(root, 112, 30, 38, 52, uic::C_CYAN);
  lTime = uic::label(root, &inter_72, uic::C_TEXT, "--:--", 160);
  lDate = uic::label(root, &inter_20, uic::C_MUTED, "", 258);
  lNext = uic::label(root, &inter_16, uic::C_CYAN, "", 300);
  return root;
}

void update(uint32_t dt) {
  eyes.update(dt);
  static const char* DAYS[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  static const char* MON[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  char buf[48];
  struct tm t;
  if (rtc_time::localTime(t)) {
    snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min);
    uic::setText(lTime, buf);
    snprintf(buf, sizeof(buf), "%s, %d %s", DAYS[t.tm_wday], t.tm_mday, MON[t.tm_mon]);
    uic::setText(lDate, buf);
    if (t.tm_sec != shownSec) { shownSec = t.tm_sec; lv_obj_invalidate(root); }
  } else {
    uic::setText(lTime, "--:--");
    uic::setText(lDate, "waiting for time");
  }
  char a[24];
  if (alarm_timer::nextAlarmText(a, sizeof(a))) snprintf(buf, sizeof(buf), "Next: alarm %s", a);
  else buf[0] = 0;
  uic::setText(lNext, buf);
}
void onShow() { shownSec = -1; }
void onHide() {}
}  // namespace ui_clock
