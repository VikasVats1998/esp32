// alarm_timer.h - alarms (days of week), 3 countdown timers, stopwatch.
// "Ringing" state is shared: touch or shake stops it (see behavior.cpp).
#pragma once
#include <Arduino.h>

namespace alarm_timer {
static const uint8_t MAX_TIMERS = 3;

void init();
void update();                         // sensor task, about every 200 ms

// ringing
bool ringing();
int ringingId();                       // 0..7 alarm, 100+ timer
void stop();                           // stop ringing now
void testRing();                       // ring for testing

// countdown timers
int startTimer(uint32_t seconds);      // returns slot or -1
void cancelTimer(uint8_t slot);
bool timerRunning(uint8_t slot);
uint32_t timerRemainingMs(uint8_t slot);
uint32_t timerTotalMs(uint8_t slot);
int activeTimer();                     // running timer with least time left, -1 none

// stopwatch
void swToggle();
void swReset();
uint32_t swElapsedMs();
bool swRunning();

bool nextAlarmText(char* out, size_t n);   // e.g. "Mon 07:30" or false if none
}  // namespace alarm_timer
