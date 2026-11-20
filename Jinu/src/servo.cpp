// servo.cpp - see servo.h
#include "servo.h"
#include "config.h"
#include "storage.h"
#include <math.h>

namespace servo {
static const uint8_t PINS[SERVO_COUNT] = {PIN_SERVO_LEFT_HIP, PIN_SERVO_RIGHT_HIP,
                                          PIN_SERVO_LEFT_ANKLE, PIN_SERVO_RIGHT_ANKLE};
struct Ch { float cur, target, speed; bool direct, known, on; uint32_t startAt; };
static Ch ch[SERVO_COUNT];
static uint32_t lastMoveMs = 0, slot = 0;

static float hwAngle(uint8_t c, float deg) {
  const ServoCfg& s = storage::get().servo[c];
  float a = (s.invert ? 180.f - deg : deg) + s.trim;
  return constrain(a, (float)s.minDeg, (float)s.maxDeg);
}

static void output(uint8_t c) {
  float us = SERVO_US_MIN + (SERVO_US_MAX - SERVO_US_MIN) * hwAngle(c, ch[c].cur) / 180.f;
  if (!ch[c].on) { ledcAttach(PINS[c], 50, 14); ch[c].on = true; }
  ledcWrite(PINS[c], (uint32_t)(us / 20000.f * 16383.f));
}

static void release(uint8_t c) {
  if (!ch[c].on) return;
  ledcDetach(PINS[c]);
  pinMode(PINS[c], OUTPUT);
  digitalWrite(PINS[c], LOW);
  ch[c].on = false;
}

void init() {
  for (uint8_t c = 0; c < SERVO_COUNT; c++) {
    ch[c] = {90, 90, SERVO_MAX_SPEED_DPS, false, false, false, 0};
    pinMode(PINS[c], OUTPUT);
    digitalWrite(PINS[c], LOW);
  }
}

void setTarget(uint8_t c, float deg, float speed) {
  if (c >= SERVO_COUNT) return;
  Ch& s = ch[c];
  s.target = constrain(deg, 0.f, 180.f);
  s.speed = speed > 0 ? fminf(speed, SERVO_MAX_SPEED_DPS) : SERVO_MAX_SPEED_DPS;
  s.direct = false;
  if (!s.known) s.speed = SERVO_SOFTSTART_DPS;
  if (!s.on) s.startAt = millis() + (slot++ % SERVO_COUNT) * SERVO_STAGGER_MS;   // spread the starts
  lastMoveMs = millis();
}

void setTargetNow(uint8_t c, float deg) {
  if (c >= SERVO_COUNT) return;
  Ch& s = ch[c];
  s.target = constrain(deg, 0.f, 180.f);
  s.direct = s.known;
  s.speed = s.known ? SERVO_MAX_SPEED_DPS : SERVO_SOFTSTART_DPS;
  lastMoveMs = millis();
}

float current(uint8_t c) { return c < SERVO_COUNT ? ch[c].cur : 90; }

bool moving() {
  for (auto& s : ch) if (fabsf(s.target - s.cur) > 0.3f) return true;
  return false;
}

void update(uint32_t dtMs) {
  uint32_t now = millis();
  float dt = dtMs / 1000.f;
  bool any = false;
  for (uint8_t c = 0; c < SERVO_COUNT; c++) {
    Ch& s = ch[c];
    float err = s.target - s.cur;
    if (s.known && fabsf(err) < 0.05f) continue;
    if ((int32_t)(now - s.startAt) < 0) continue;
    if (!s.known) { s.cur = s.target; s.known = true; output(c); any = true; continue; }
    float step = s.speed * dt;
    float ease = s.direct ? 1.f : fminf(1.f, 0.25f + fabsf(err) / 15.f);   // slow down near the target
    float mv = fminf(fabsf(err), step * ease);
    s.cur += err > 0 ? mv : -mv;
    output(c);
    any = true;
  }
  if (any) lastMoveMs = now;
  if (now - lastMoveMs > SERVO_IDLE_OFF_MS && !moving()) releaseAll();
}

void releaseAll() { for (uint8_t c = 0; c < SERVO_COUNT; c++) release(c); }

void hold(uint8_t c) {
  if (c >= SERVO_COUNT) return;
  ch[c].known = true;
  ch[c].cur = ch[c].target;
  output(c);
  lastMoveMs = millis();
}
}  // namespace servo
