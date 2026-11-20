// pads.cpp - see pads.h
#include "pads.h"
#include "config.h"
#include "bus.h"
#include "events.h"

namespace pads {

static const uint16_t DEBOUNCE_MS = 40;
static const uint16_t TAP_GAP_MS = 320;       // wait this long for the next tap
static const uint16_t LONG_MS = 650;

struct Pad {
  bool raw, stable, longFired;
  uint32_t changedAt, downAt, upAt;
  uint8_t taps;
};
static Pad P[PAD_COUNT];
static bool pcfOk = false;
static uint8_t pcfBit[4] = {PCF_BIT_EAR_L, PCF_BIT_EAR_R, PCF_BIT_LEG_L, PCF_BIT_LEG_R};

const char* padName(uint8_t p) {
  static const char* N[PAD_COUNT] = {"ear L", "ear R", "leg L", "leg R", "body"};
  return p < PAD_COUNT ? N[p] : "?";
}
bool expanderOnline() { return pcfOk; }

uint8_t rawState() {
  uint8_t r = 0;
  for (int i = 0; i < PAD_COUNT; i++) if (P[i].stable) r |= 1 << i;
  return r;
}

bool init() {
  pinMode(PIN_TTP223, INPUT_PULLDOWN);
  bus::Guard1 g;
  Wire1.beginTransmission(I2C_ADDR_PCF8574);
  Wire1.write(0xFF);                          // all pins high = inputs (quasi-bidirectional)
  pcfOk = Wire1.endTransmission() == 0;
  return pcfOk;
}

static uint8_t readExpander() {
  bus::Guard1 g;
  if (Wire1.requestFrom((int)I2C_ADDR_PCF8574, 1) != 1) return 0;
  return Wire1.read();
}

static void publish(uint8_t pad, Gesture g) { events::publish(events::EV_PAD, pad, g); }

void update() {
  uint32_t now = millis();
  uint8_t ex = pcfOk ? readExpander() : 0;
  for (uint8_t i = 0; i < PAD_COUNT; i++) {
    Pad& p = P[i];
    bool raw = (i == PAD_BODY) ? digitalRead(PIN_TTP223) == HIGH : ((ex >> pcfBit[i]) & 1);
    if (raw != p.raw) { p.raw = raw; p.changedAt = now; }
    if (raw != p.stable && now - p.changedAt >= DEBOUNCE_MS) {
      p.stable = raw;
      if (raw) { p.downAt = now; p.longFired = false; }
      else if (p.longFired) { publish(i, LONG_END); p.taps = 0; }
      else { p.taps++; p.upAt = now; }
    }
    if (p.stable && !p.longFired && now - p.downAt > LONG_MS) {
      p.longFired = true;
      p.taps = 0;
      publish(i, LONG_START);
    }
    if (!p.stable && p.taps) {
      if (p.taps >= 3) { publish(i, TAP3); p.taps = 0; }                 // triple: no need to wait
      else if (now - p.upAt > TAP_GAP_MS) { publish(i, (Gesture)p.taps); p.taps = 0; }
    }
  }
}

}  // namespace pads
