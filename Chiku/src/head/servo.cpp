// servo.cpp - see servo.h
#include "servo.h"
#include "config.h"
#include "storage.h"
#include <Wire.h>
#include <math.h>

namespace servo {

// PCA9685 registers
static const uint8_t MODE1 = 0x00, MODE2 = 0x01, PRESCALE = 0xFE, LED0_ON_L = 0x06;

static bool ok = false;
static bool asleep = true;
static bool everMoved = false;

struct Ch {
  float cur;          // current angle (deg)
  float target;       // wanted angle (deg)
  float speed;        // deg/s limit for this move
  bool direct;        // oscillator mode: no easing
  uint32_t startAt;   // millis() when this servo may start (stagger)
  bool known;         // false until the first command (soft start)
  bool on;            // PWM currently enabled
};
static Ch ch[SERVO_COUNT];
static uint32_t lastMoveMs = 0;
static uint32_t staggerSlot = 0;
static SemaphoreHandle_t mtx = nullptr;   // update() and calibration may run in different tasks
struct Lock { Lock() { if (mtx) xSemaphoreTakeRecursive(mtx, portMAX_DELAY); } ~Lock() { if (mtx) xSemaphoreGiveRecursive(mtx); } };

static bool write8(uint8_t reg, uint8_t v) {
  Wire1.beginTransmission(I2C_ADDR_PCA9685);
  Wire1.write(reg); Wire1.write(v);
  return Wire1.endTransmission() == 0;
}

static void writePwm(uint8_t c, uint16_t on, uint16_t off) {
  Wire1.beginTransmission(I2C_ADDR_PCA9685);
  Wire1.write(LED0_ON_L + 4 * c);
  Wire1.write(on & 255); Wire1.write(on >> 8);
  Wire1.write(off & 255); Wire1.write(off >> 8);
  Wire1.endTransmission();
}

static void channelOff(uint8_t c) {
  writePwm(c, 0, 0x1000);               // "full off" bit
  ch[c].on = false;
}

static void chipSleep() {
  if (asleep) return;
  write8(MODE1, 0x10);                  // SLEEP bit, oscillator off
  asleep = true;
}

static void chipWake() {
  if (!asleep) return;
  write8(MODE1, 0x20);                  // auto-increment, oscillator on
  delay(1);                             // oscillator needs 500 us
  write8(MODE1, 0xA0);                  // restart
  asleep = false;
  if (PIN_PCA_OE >= 0) digitalWrite(PIN_PCA_OE, LOW);
}

static uint16_t angleToTicks(float deg) {
  deg = constrain(deg, 0.f, 180.f);
  float us = SERVO_US_MIN + (SERVO_US_MAX - SERVO_US_MIN) * deg / 180.f;
  return (uint16_t)(us * 4096.f * SERVO_FREQ_HZ / 1000000.f + 0.5f);
}

// Convert logical angle -> hardware angle with invert, trim and limits.
static float hwAngle(uint8_t c, float deg) {
  const ServoCfg& s = storage::get().servo[c];
  float a = s.invert ? 180.f - deg : deg;
  a += s.trim;
  return constrain(a, (float)s.minDeg, (float)s.maxDeg);
}

static void output(uint8_t c) {
  writePwm(c, 0, angleToTicks(hwAngle(c, ch[c].cur)));
  ch[c].on = true;
}

bool init() {
  if (PIN_PCA_OE >= 0) { pinMode(PIN_PCA_OE, OUTPUT); digitalWrite(PIN_PCA_OE, HIGH); }
  mtx = xSemaphoreCreateRecursiveMutex();
  Wire1.begin(PIN_I2C1_SDA, PIN_I2C1_SCL, 400000);
  Wire1.setTimeOut(20);
  Wire1.beginTransmission(I2C_ADDR_PCA9685);
  ok = Wire1.endTransmission() == 0;
  if (!ok) return false;
  write8(MODE1, 0x10);                                  // sleep to set prescale
  write8(PRESCALE, (uint8_t)(25000000.0f / (4096.0f * SERVO_FREQ_HZ) - 1.0f + 0.5f));
  write8(MODE2, 0x04);                                  // totem pole outputs
  asleep = true;
  for (uint8_t i = 0; i < SERVO_COUNT; i++) {
    ch[i] = {90, 90, SERVO_MAX_SPEED_DPS, false, 0, false, false};
  }
  return true;
}

bool online() { return ok; }

void setTarget(uint8_t c, float deg, float speedDps) {
  if (!ok || c >= SERVO_COUNT) return;
  Ch& s = ch[c];
  s.target = constrain(deg, 0.f, 180.f);
  s.speed = speedDps > 0 ? fminf(speedDps, SERVO_MAX_SPEED_DPS) : SERVO_MAX_SPEED_DPS;
  s.direct = false;
  if (!s.known) {                       // soft start: first move is slow
    s.speed = SERVO_SOFTSTART_DPS;
  }
  // Stagger: a servo that was idle waits a little so starts are spread out.
  if (!s.on || fabsf(s.target - s.cur) < 0.5f) {
    s.startAt = millis() + (staggerSlot++ % SERVO_COUNT) * SERVO_STAGGER_MS;
  }
  lastMoveMs = millis();
}

void setTargetNow(uint8_t c, float deg) {
  if (!ok || c >= SERVO_COUNT) return;
  Ch& s = ch[c];
  s.target = constrain(deg, 0.f, 180.f);
  s.direct = true;
  s.speed = SERVO_MAX_SPEED_DPS;
  if (!s.known) { s.direct = false; s.speed = SERVO_SOFTSTART_DPS; }
  lastMoveMs = millis();
}

float current(uint8_t c) { return c < SERVO_COUNT ? ch[c].cur : 90; }

bool moving() {
  for (auto& s : ch) if (fabsf(s.target - s.cur) > 0.3f) return true;
  return false;
}

void update(uint32_t dtMs) {
  if (!ok) return;
  Lock lk;
  uint32_t now = millis();
  float dt = dtMs / 1000.f;
  bool any = false;
  for (uint8_t c = 0; c < SERVO_COUNT; c++) {
    Ch& s = ch[c];
    float err = s.target - s.cur;
    if (fabsf(err) < 0.05f && s.known) continue;
    if ((int32_t)(now - s.startAt) < 0) continue;
    if (asleep) chipWake();
    if (!s.known) {                      // first command ever: jump to start angle, then glide
      s.cur = s.target; s.known = true;  // we do not know the real position
      output(c);
      any = true;
      continue;
    }
    // Speed limit + ease-out near the target (slows down when closer than 15 deg)
    float step = s.speed * dt;
    float ease = fminf(1.f, 0.25f + fabsf(err) / 15.f);
    if (s.direct) { step = fabsf(err); ease = 1; }
    float move = fminf(fabsf(err), step * ease);
    s.cur += err > 0 ? move : -move;
    output(c);
    any = true;
  }
  if (any) { lastMoveMs = now; everMoved = true; }
  // Idle for 2 s -> all outputs off (no holding current), chip to sleep
  if (!asleep && now - lastMoveMs > SERVO_IDLE_OFF_MS && !moving()) {
    for (uint8_t c = 0; c < SERVO_COUNT; c++) channelOff(c);
    chipSleep();
  }
}

void releaseAll() {
  if (!ok) return;
  Lock lk;
  for (uint8_t c = 0; c < SERVO_COUNT; c++) { ch[c].target = ch[c].cur; channelOff(c); }
  chipSleep();
}

void freeze() { releaseAll(); }

void applySettings() {
  Lock lk;
  for (uint8_t c = 0; c < SERVO_COUNT; c++) if (ch[c].on) output(c);
}

void hold(uint8_t c) {
  if (!ok || c >= SERVO_COUNT) return;
  Lock lk;
  if (asleep) chipWake();
  ch[c].known = true;
  ch[c].cur = ch[c].target;
  output(c);
  lastMoveMs = millis();
}

void rawPulse(uint8_t c, float deg) {
  if (!ok || c >= SERVO_COUNT) return;
  Lock lk;
  if (asleep) chipWake();
  writePwm(c, 0, angleToTicks(deg));
  ch[c].on = true;
  ch[c].cur = ch[c].target = deg;
  ch[c].known = true;
  lastMoveMs = millis();
}

}  // namespace servo
