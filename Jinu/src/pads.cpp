// pads.cpp - see pads.h (same gesture logic as Chiku)
#include "pads.h"
#include "config.h"
#include <Wire.h>
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
bool online() { return pcfOk; }

uint8_t rawState() {
  uint8_t r = 0;
  for (int i = 0; i < PAD_COUNT; i++) if (P[i].stable) r |= 1 << i;
  return r;
}

bool init() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000);
  Wire.setTimeOut(20);
  pinMode(PIN_PAD_INT, INPUT_PULLUP);
  Wire.beginTransmission(I2C_ADDR_PCF8574);
  Wire.write(0xFF);                           // all pins high = inputs (quasi-bidirectional)
  pcfOk = Wire.endTransmission() == 0;
  return pcfOk;
}

static uint8_t readExpander() {
  if (Wire.requestFrom((int)I2C_ADDR_PCF8574, 1) != 1) return 0;
  return Wire.read();
}

static void publish(uint8_t pad, Gesture g) { events::publish(events::EV_PAD, pad, g); }

void update() {
  uint32_t now = millis();
  uint8_t ex = pcfOk ? readExpander() : 0;
  for (uint8_t i = 0; i < PAD_COUNT; i++) {
    Pad& p = P[i];
    bool raw = (ex >> i) & 1;                     // pad n = PCF8574 bit n
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
