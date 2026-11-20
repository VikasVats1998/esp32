// ui.cpp - see ui.h. LED: purple = pairing, yellow = calibrating, green = linked, blue = paired but no link,
// pink = not paired, red = gimbal failsafe / low battery blink; a short white flash while a macro runs on the gimbal.
#include "ui.h"
#include "config.h"
#include "inputs.h"
#include "rlink.h"

namespace ui {

static int mv = 4000;
static uint32_t lastBatt = 0;

void init() {
#if PIN_BUZZER >= 0
  pinMode(PIN_BUZZER, OUTPUT);
#endif
}

void beep(uint16_t hz, uint16_t ms) {
#if PIN_BUZZER >= 0
  tone(PIN_BUZZER, hz, ms);
#endif
}
void signalOk() { beep(1200, 80); }
void signalError() { beep(300, 250); }

int batteryMv() { return mv; }
int batteryPercent() { return constrain((mv - BATT_LOW_MV) * 100 / (BATT_FULL_MV - BATT_LOW_MV), 0, 100); }
bool batteryLow() { return mv < BATT_LOW_MV; }

void update() {
  uint32_t now = millis();
  if (now - lastBatt > 2000) {
    lastBatt = now;
    long sum = 0;
    for (int i = 0; i < 8; i++) sum += analogReadMilliVolts(PIN_BATT);
    mv = (int)(sum / 8 * 2);                                   // 100k / 100k divider
  }
#if PIN_STATUS_LED >= 0
  static uint32_t lastLed = 0;
  if (now - lastLed < 80) return;
  lastLed = now;
  uint8_t r = 0, g = 0, b = 0;
  if (rlink::pairing()) { r = 25; b = 40; }
  else if (inputs::calibrating()) { r = 40; g = 30; }
  else if (!rlink::paired()) { r = 30; b = 12; }
  else if (!rlink::linkOk()) b = 25;
  else if (rlink::gimbalFailsafe()) r = 40;
  else if (rlink::gimbalMacroRunning()) { r = 30; g = 30; b = 30; }
  else if (inputs::state().fine) { g = 12; b = 12; }
  else g = 14;
  if (batteryLow() && (now / 500) % 2) { r = 40; g = 10; b = 0; }          // low battery: orange blink
  rgbLedWrite(PIN_STATUS_LED, r, g, b);
#endif
}

}  // namespace ui
