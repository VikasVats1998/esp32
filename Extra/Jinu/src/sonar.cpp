// sonar.cpp - see sonar.h
#include "sonar.h"
#include "config.h"
#include "logbuf.h"
#include "events.h"

namespace sonar {
static volatile uint32_t tRise = 0, tFall = 0;
static volatile bool done = false;
static int buf[SONAR_MEDIAN_N];
static uint8_t count = 0, head = 0;
static int filtered = -1;
static uint32_t lastPing = 0, lastGood = 0;
static bool near = false;

static void IRAM_ATTR onEcho() {
  if (digitalRead(PIN_SONAR_ECHO)) tRise = micros();
  else { tFall = micros(); done = true; }
}

void init() {
  pinMode(PIN_SONAR_TRIG, OUTPUT);
  pinMode(PIN_SONAR_ECHO, INPUT);
  attachInterrupt(digitalPinToInterrupt(PIN_SONAR_ECHO), onEcho, CHANGE);
}

int distanceCm() { return (millis() - lastGood < 1000) ? filtered : -1; }

static int median() {
  int t[SONAR_MEDIAN_N];
  memcpy(t, buf, sizeof(int) * count);
  for (int i = 1; i < count; i++) { int v = t[i], j = i - 1; while (j >= 0 && t[j] > v) { t[j + 1] = t[j]; j--; } t[j + 1] = v; }
  return t[count / 2];
}

void update() {
  uint32_t now = millis();
  if (done) {
    done = false;
    uint32_t us = tFall - tRise;
    int cm = (int)(us / 58);
    if (cm > 1 && cm < 400) {
      buf[head] = cm; head = (head + 1) % SONAR_MEDIAN_N; if (count < SONAR_MEDIAN_N) count++;
      lastGood = now;
      int m = median();
      if (m != filtered) { filtered = m; events::publish(events::EV_DIST, m); }
      bool n = m < OBSTACLE_STOP_CM;
      if (n && !near) events::publish(events::EV_OBSTACLE, m);
      near = n;
    }
  }
  if (now - lastPing >= 60) {
    lastPing = now;
    digitalWrite(PIN_SONAR_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_SONAR_TRIG, LOW);
  }
}
}  // namespace sonar
