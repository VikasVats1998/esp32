// ui_timer.cpp - focus timer (UI kit): orange ring, mini eyes, big countdown, Pomodoro count,
// pause button. No timer running: pick 5, 10 or 25 (Pomodoro) minutes. Chiku dances when it ends.
#include "ui_screens.h"
#include "ui_common.h"
#include "alarm_timer.h"
#include "audio.h"

namespace ui_timer {
static lv_obj_t *ring, *lTime, *lSub, *presets, *running, *bPause;
static uic::MiniEyes eyes;

static void onPreset(lv_event_t* e) {
  alarm_timer::startTimer((uint32_t)(uintptr_t)lv_event_get_user_data(e));
  audio::play(audio::MEL_CLICK);
}
static void onPause(lv_event_t*) {
  int s = alarm_timer::activeTimer();
  if (s >= 0) alarm_timer::pauseTimer(s, !alarm_timer::timerPaused(s));
}
static void onCancel(lv_event_t*) {
  int s = alarm_timer::activeTimer();
  if (s >= 0) alarm_timer::cancelTimer(s);
}

lv_obj_t* create(lv_obj_t* parent) {
  lv_obj_t* p = uic::panel(parent);
  ring = uic::ring(p, 420, 14, uic::C_ORANGE);
  eyes.create(p, 112, 26, 34, 48, uic::C_ORANGE);
  lTime = uic::label(p, &inter_72, uic::C_TEXT, "00:00", 160);
  lSub = uic::label(p, &inter_16, uic::C_MUTED, "", 256);
  presets = uic::plain(p);
  lv_obj_set_size(presets, LCD_W, 60);
  lv_obj_set_pos(presets, 0, 292);
  uic::button(presets, "5 min", 83, 0, 92, 48, onPreset, (void*)300);
  uic::button(presets, "10 min", 187, 0, 92, 48, onPreset, (void*)600);
  uic::button(presets, "25 min", 291, 0, 92, 48, onPreset, (void*)1500, uic::C_ORANGE, 0x000000);
  running = uic::plain(p);
  lv_obj_set_size(running, LCD_W, 60);
  lv_obj_set_pos(running, 0, 290);
  bPause = uic::circleButton(running, LV_SYMBOL_PAUSE, 205, 28, 56, uic::C_TEXT, onPause, nullptr, &inter_20);
  lv_obj_set_style_border_width(bPause, 0, 0);
  lv_obj_set_style_bg_color(bPause, lv_color_hex(0x2A3038), 0);
  lv_obj_t* x = uic::circleButton(running, LV_SYMBOL_CLOSE, 275, 28, 44, uic::C_MUTED, onCancel, nullptr, &inter_16);
  lv_obj_set_style_border_width(x, 0, 0);
  return p;
}

void update(uint32_t dt) {
  eyes.update(dt);
  char buf[40];
  int s = alarm_timer::activeTimer();
  uic::show(presets, s < 0);
  uic::show(running, s >= 0);
  if (s < 0) {
    uic::setText(lTime, "00:00");
    snprintf(buf, sizeof(buf), "Focus \xC2\xB7 Pomodoro %u/4", (unsigned)min<uint8_t>(alarm_timer::pomodoros(), 4));
    uic::setText(lSub, alarm_timer::pomodoros() ? buf : "Pick a time");
    uic::setRing(ring, 0);
    return;
  }
  uint32_t left = (alarm_timer::timerRemainingMs(s) + 999) / 1000, total = alarm_timer::timerTotalMs(s);
  snprintf(buf, sizeof(buf), "%02u:%02u", (unsigned)(left / 60), (unsigned)(left % 60));
  uic::setText(lTime, buf);
  if (total == 25UL * 60000UL) snprintf(buf, sizeof(buf), "Focus \xC2\xB7 Pomodoro %u/4", (unsigned)min(alarm_timer::pomodoros() + 1, 4));
  else snprintf(buf, sizeof(buf), "Timer \xC2\xB7 %lu min", (unsigned long)(total / 60000));
  uic::setText(lSub, alarm_timer::timerPaused(s) ? "Paused" : buf);
  uic::setRing(ring, total ? (int)(1000ULL * alarm_timer::timerRemainingMs(s) / total) : 0);
  uic::setText(lv_obj_get_child(bPause, 0), alarm_timer::timerPaused(s) ? LV_SYMBOL_PLAY : LV_SYMBOL_PAUSE);
}
void onShow() {}
void onHide() {}
}  // namespace ui_timer
