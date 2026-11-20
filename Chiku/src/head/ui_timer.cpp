// ui_timer.cpp - countdown timers (presets) and stopwatch.
#include "ui_screens.h"
#include "ui_common.h"
#include "alarm_timer.h"
#include "audio.h"

namespace ui_timer {
static lv_obj_t *lTimer, *lWatch, *bWatch;

static void onPreset(lv_event_t* e) {
  uint32_t sec = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
  alarm_timer::startTimer(sec);
  audio::play(audio::MEL_CLICK);
}
static void onCancel(lv_event_t*) {
  int s = alarm_timer::activeTimer();
  if (s >= 0) alarm_timer::cancelTimer(s);
}
static void onWatch(lv_event_t*) { alarm_timer::swToggle(); }
static void onReset(lv_event_t*) { alarm_timer::swReset(); }

lv_obj_t* create(lv_obj_t* parent) {
  lv_obj_t* p = uic::panel(parent);
  uic::label(p, &lv_font_montserrat_14, 0x808080, "TIMER", 60);
  lTimer = uic::label(p, &lv_font_montserrat_48, 0xFFB040, "--:--", 82);
  uic::button(p, "1 min", 63, 150, 100, 48, onPreset, (void*)60);
  uic::button(p, "5 min", 183, 150, 100, 48, onPreset, (void*)300);
  uic::button(p, "10 min", 303, 150, 100, 48, onPreset, (void*)600);
  uic::button(p, "Cancel", 183, 205, 100, 40, onCancel, nullptr, 0x703030);
  uic::label(p, &lv_font_montserrat_14, 0x808080, "STOPWATCH", 270);
  lWatch = uic::label(p, &lv_font_montserrat_28, 0xFFFFFF, "00:00.0", 292);
  bWatch = uic::button(p, "Start", 113, 340, 110, 46, onWatch, nullptr, 0x2A6A3A);
  uic::button(p, "Reset", 243, 340, 110, 46, onReset);
  return p;
}

void update() {
  char buf[24];
  int s = alarm_timer::activeTimer();
  if (s >= 0) {
    uint32_t left = alarm_timer::timerRemainingMs(s) / 1000;
    snprintf(buf, sizeof(buf), "%02u:%02u", (unsigned)(left / 60), (unsigned)(left % 60));
    uic::setText(lTimer, buf);
  } else uic::setText(lTimer, "--:--");
  uint32_t ms = alarm_timer::swElapsedMs();
  snprintf(buf, sizeof(buf), "%02u:%02u.%u", (unsigned)(ms / 60000), (unsigned)((ms / 1000) % 60), (unsigned)((ms / 100) % 10));
  uic::setText(lWatch, buf);
  uic::setButtonText(bWatch, alarm_timer::swRunning() ? "Stop" : "Start");
}
}  // namespace ui_timer
