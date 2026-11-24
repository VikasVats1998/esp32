// alarm_timer.cpp - see alarm_timer.h
#include "alarm_timer.h"
#include "config.h"
#include "events.h"
#include "rtc_time.h"
#include "storage.h"

namespace alarm_timer {

struct Timer { bool run; uint32_t endMs; uint32_t totalMs; bool paused; uint32_t leftMs; };
static Timer timers[MAX_TIMERS];
static bool swRun = false;
static uint32_t swStart = 0, swAcc = 0;
static bool isRinging = false;
static int ringId = -1;
static uint32_t ringStart = 0;
static int lastMinuteKey = -1;
static uint32_t snoozeAt = 0;
static int snoozeId = -1;
static uint8_t pomoCount = 0;
static int pomoDay = -1;

void init() {}

bool ringing() { return isRinging; }
int ringingId() { return ringId; }

static void startRing(int id) {
  if (isRinging) return;
  isRinging = true; ringId = id; ringStart = millis();
  events::publish(events::EV_ALARM_RING, id);
}

void testRing() { startRing(0); }

void snooze(uint8_t minutes) {
  if (!isRinging) return;
  snoozeId = ringId;
  snoozeAt = millis() + minutes * 60000UL;
  stop();
}

void stop() {
  if (!isRinging) return;
  isRinging = false;
  events::publish(events::EV_ALARM_STOP, ringId);
}

int startTimer(uint32_t seconds) {
  for (uint8_t i = 0; i < MAX_TIMERS; i++) {
    if (!timers[i].run) {
      timers[i] = {true, millis() + seconds * 1000UL, seconds * 1000UL, false, 0};
      return i;
    }
  }
  return -1;
}
void cancelTimer(uint8_t s) { if (s < MAX_TIMERS) timers[s].run = false; }
bool timerRunning(uint8_t s) { return s < MAX_TIMERS && timers[s].run; }
uint32_t timerRemainingMs(uint8_t s) {
  if (s >= MAX_TIMERS || !timers[s].run) return 0;
  if (timers[s].paused) return timers[s].leftMs;
  int32_t left = (int32_t)(timers[s].endMs - millis());
  return left > 0 ? left : 0;
}
uint32_t timerTotalMs(uint8_t s) { return s < MAX_TIMERS ? timers[s].totalMs : 0; }
int activeTimer() {
  int best = -1; uint32_t bestLeft = 0xFFFFFFFF;
  for (uint8_t i = 0; i < MAX_TIMERS; i++) {
    if (timers[i].run && timerRemainingMs(i) < bestLeft) { bestLeft = timerRemainingMs(i); best = i; }
  }
  return best;
}

void pauseTimer(uint8_t s, bool pause) {
  if (s >= MAX_TIMERS || !timers[s].run || timers[s].paused == pause) return;
  if (pause) { timers[s].leftMs = timerRemainingMs(s); timers[s].paused = true; }
  else { timers[s].endMs = millis() + timers[s].leftMs; timers[s].paused = false; }
}
bool timerPaused(uint8_t s) { return s < MAX_TIMERS && timers[s].run && timers[s].paused; }
uint8_t pomodoros() { return pomoCount; }

void swToggle() {
  if (swRun) { swAcc += millis() - swStart; swRun = false; }
  else { swStart = millis(); swRun = true; }
}
void swReset() { swRun = false; swAcc = 0; }
uint32_t swElapsedMs() { return swAcc + (swRun ? millis() - swStart : 0); }
bool swRunning() { return swRun; }

void update() {
  uint32_t now = millis();
  // timers
  for (uint8_t i = 0; i < MAX_TIMERS; i++) {
    if (timers[i].run && !timers[i].paused && (int32_t)(now - timers[i].endMs) >= 0) {
      timers[i].run = false;
      if (timers[i].totalMs == 25UL * 60000UL) pomoCount++;
      events::publish(events::EV_TIMER_DONE, i);
      startRing(100 + i);
    }
  }
  // snoozed alarm comes back
  if (snoozeAt && (int32_t)(now - snoozeAt) >= 0) { snoozeAt = 0; startRing(snoozeId); }
  // alarms: check once per minute
  struct tm t;
  if (rtc_time::localTime(t)) {
    if (t.tm_yday != pomoDay) { pomoDay = t.tm_yday; pomoCount = 0; }
    int key = t.tm_yday * 1440 + t.tm_hour * 60 + t.tm_min;
    if (key != lastMinuteKey) {
      lastMinuteKey = key;
      int dayBit = (t.tm_wday + 6) % 7;                  // Mon = 0
      Settings& s = storage::get();
      for (int i = 0; i < MAX_ALARMS; i++) {
        const AlarmCfg& a = s.alarms[i];
        if (a.enabled && (a.days & (1 << dayBit)) && a.hour == t.tm_hour && a.minute == t.tm_min) {
          startRing(i);
          break;
        }
      }
    }
  }
  // give up after 2 minutes
  if (isRinging && now - ringStart > 120000UL) stop();
}

bool nextAlarmText(char* out, size_t n) {
  struct tm t;
  if (!rtc_time::localTime(t)) return false;
  static const char* DAYS[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
  Settings& s = storage::get();
  int nowMin = t.tm_hour * 60 + t.tm_min;
  int today = (t.tm_wday + 6) % 7;
  for (int off = 0; off < 8; off++) {
    int day = (today + off) % 7;
    int bestMin = 99999, bestIdx = -1;
    for (int i = 0; i < MAX_ALARMS; i++) {
      const AlarmCfg& a = s.alarms[i];
      if (!a.enabled || !(a.days & (1 << day))) continue;
      int m = a.hour * 60 + a.minute;
      if (off == 0 && m <= nowMin) continue;
      if (m < bestMin) { bestMin = m; bestIdx = i; }
    }
    if (bestIdx >= 0) {
      snprintf(out, n, "%s %02d:%02d", DAYS[day], bestMin / 60, bestMin % 60);
      return true;
    }
  }
  return false;
}

}  // namespace alarm_timer
