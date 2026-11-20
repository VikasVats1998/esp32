// power.cpp - see power.h
#include "power.h"
#include "config.h"
#include "logbuf.h"
#include "bus.h"
#include "events.h"
#define XPOWERS_CHIP_AXP2101
#include <XPowersLib.h>

namespace power {

static XPowersPMU pmu;
static bool pmuOk = false;
static State st = ACTIVE;
static volatile uint32_t lastActivity = 0;
static volatile bool awake = false;
static int pct = -1;
static bool chg = false;
static float volts = 0;
static uint32_t lastBatt = 0;

static void setState(State s) {
  if (s == st) return;
  st = s;
  LOGI("power", "state -> %s", s == ACTIVE ? "active" : s == DIM ? "dim" : "sleep");
  setCpuFrequencyMhz(s == SLEEP ? 80 : 160);      // slower CPU while sleeping
  events::publish(events::EV_POWER, s);
}

void init() {
  lastActivity = millis();
  setCpuFrequencyMhz(160);
#if FEATURE_PMU
  bus::Guard g;
  pmuOk = pmu.begin(Wire, I2C_ADDR_AXP2101, PIN_I2C0_SDA, PIN_I2C0_SCL);
  if (pmuOk) pmu.enableBattVoltageMeasure();
#endif
}

void activity() {
  lastActivity = millis();
  if (st != ACTIVE) {
    setState(ACTIVE);
    events::publish(events::EV_WAKE);
  }
}

void keepAwake(bool on) { awake = on; if (on) activity(); }
State state() { return st; }
int batteryPercent() { return pct; }
bool charging() { return chg; }
bool pmuOnline() { return pmuOk; }
float batteryVolts() { return volts; }

void update() {
  uint32_t now = millis();
  if (!awake) {
    uint32_t idle = now - lastActivity;
    if (idle > SLEEP_AFTER_MS) setState(SLEEP);
    else if (idle > DIM_AFTER_MS) setState(DIM);
  }
  if (pmuOk && now - lastBatt > 5000) {
    lastBatt = now;
    bus::Guard g;
    if (pmu.isBatteryConnect()) {
      pct = pmu.getBatteryPercent();
      volts = pmu.getBattVoltage() / 1000.f;
    } else pct = -1;
    chg = pmu.isCharging();
  }
}

}  // namespace power
