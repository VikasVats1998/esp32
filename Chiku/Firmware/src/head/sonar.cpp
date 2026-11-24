// sonar.cpp - see sonar.h
#include "sonar.h"
#include "config.h"
#include "logbuf.h"
#include "events.h"

namespace sonar {

static QueueHandle_t q = nullptr;
static int buf[SONAR_MEDIAN_N];
static uint8_t count = 0, head = 0;
static int filtered = -1;
static uint32_t lastSample = 0;
static bool near = false;

void init() {
  q = events::subscribe(24);
#if PIN_SONAR_TRIG >= 0 && PIN_SONAR_ECHO >= 0
  pinMode(PIN_SONAR_TRIG, OUTPUT);
  pinMode(PIN_SONAR_ECHO, INPUT);
#endif
}

int distanceCm() { return (millis() - lastSample < 1500) ? filtered : -1; }
uint32_t ageMs() { return millis() - lastSample; }

static int median() {
  int tmp[SONAR_MEDIAN_N];
  memcpy(tmp, buf, sizeof(int) * count);
  for (int i = 1; i < count; i++) {          // tiny insertion sort
    int v = tmp[i], j = i - 1;
    while (j >= 0 && tmp[j] > v) { tmp[j + 1] = tmp[j]; j--; }
    tmp[j + 1] = v;
  }
  return tmp[count / 2];
}

static void addSample(int cm) {
  if (cm <= 0 || cm > 400) return;           // invalid
  buf[head] = cm;
  head = (head + 1) % SONAR_MEDIAN_N;
  if (count < SONAR_MEDIAN_N) count++;
  lastSample = millis();
  int m = median();
  if (m != filtered) {
    filtered = m;
    events::publish(events::EV_DIST, m);
  }
  bool nowNear = m < OBSTACLE_STOP_CM;
  if (nowNear && !near) events::publish(events::EV_OBSTACLE, m);
  near = nowNear;
}

void update() {
#if PIN_SONAR_TRIG >= 0 && PIN_SONAR_ECHO >= 0
  static uint32_t lastPing = 0;
  if (millis() - lastPing > 100) {
    lastPing = millis();
    digitalWrite(PIN_SONAR_TRIG, LOW); delayMicroseconds(2);
    digitalWrite(PIN_SONAR_TRIG, HIGH); delayMicroseconds(10);
    digitalWrite(PIN_SONAR_TRIG, LOW);
    unsigned long us = pulseIn(PIN_SONAR_ECHO, HIGH, 25000);
    if (us) addSample((int)(us / 58));
  }
#endif
  events::Event e;
  while (xQueueReceive(q, &e, 0) == pdTRUE) {
    if (e.type == events::EV_DIST_RAW) addSample(e.a);
  }
}

}  // namespace sonar
