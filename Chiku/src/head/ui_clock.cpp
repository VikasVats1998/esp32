// ui_clock.cpp - clock screen: time, date, next alarm, status line.
#include "ui_screens.h"
#include "ui_common.h"
#include "rtc_time.h"
#include "alarm_timer.h"
#include "power.h"
#include "net.h"

namespace ui_clock {
static lv_obj_t *lTime, *lSec, *lDate, *lAlarm, *lStatus;

lv_obj_t* create(lv_obj_t* parent) {
  lv_obj_t* p = uic::panel(parent);
  lTime = uic::label(p, &lv_font_montserrat_48, 0xFFFFFF, "--:--", 140);
  lSec = uic::label(p, &lv_font_montserrat_28, 0x5CE1FF, "", 205);
  lDate = uic::label(p, &lv_font_montserrat_20, 0xA0A0A0, "", 255);
  lAlarm = uic::label(p, &lv_font_montserrat_20, 0xFFB040, "", 300);
  lStatus = uic::label(p, &lv_font_montserrat_14, 0x707070, "", 360);
  return p;
}

void update() {
  static const char* DAYS[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
  static const char* MON[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  char buf[48];
  struct tm t;
  if (rtc_time::localTime(t)) {
    snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min);
    uic::setText(lTime, buf);
    snprintf(buf, sizeof(buf), ":%02d", t.tm_sec);
    uic::setText(lSec, buf);
    snprintf(buf, sizeof(buf), "%s, %d %s %d", DAYS[t.tm_wday], t.tm_mday, MON[t.tm_mon], t.tm_year + 1900);
    uic::setText(lDate, buf);
  } else {
    uic::setText(lTime, "--:--");
    uic::setText(lSec, "waiting for time");
    uic::setText(lDate, "");
  }
  char a[24];
  if (alarm_timer::nextAlarmText(a, sizeof(a))) snprintf(buf, sizeof(buf), LV_SYMBOL_BELL " %s", a);
  else buf[0] = 0;
  uic::setText(lAlarm, buf);
  int pct = power::batteryPercent();
  snprintf(buf, sizeof(buf), "%s   %s%d%%%s", net::connected() ? LV_SYMBOL_WIFI : "no wifi",
           LV_SYMBOL_BATTERY_FULL " ", pct < 0 ? 0 : pct, power::charging() ? " " LV_SYMBOL_CHARGE : "");
  uic::setText(lStatus, buf);
}
}  // namespace ui_clock
