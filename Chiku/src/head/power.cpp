// power.cpp - see power.h
#include "power.h"
#include "config.h"
#include "logbuf.h"
#include "bus.h"
#include "events.h"
#include "servo.h"
#include "display.h"
#include "audio.h"
#include <esp_sleep.h>
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
static float volts = 0, vbus = 0;
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
  for (int attempt = 0; attempt < 4 && !pmuOk; attempt++) {      // the chip sometimes misses the first try
    if (attempt) delay(40);
    pmuOk = pmu.begin(Wire, I2C_ADDR_AXP2101, PIN_I2C0_SDA, PIN_I2C0_SCL);
  }
  if (pmuOk) { pmu.enableBattDetection(); pmu.enableBattVoltageMeasure(); pmu.enableVbusVoltageMeasure(); }
  else LOGW("power", "AXP2101 not answering");
#endif
}

void shutdown() {
  LOGI("power", "power off");
  servo::releaseAll();                                  // legs limp, no holding current
  audio::beep(700, 120); delay(160); audio::beep(450, 160); delay(220);
  display::setBrightness(0);
  display::panelOff();
#if FEATURE_PMU
  if (pmuOk) { bus::Guard g; pmu.shutdown(); }          // cuts every rail (battery or no USB: this is the end)
#endif
  delay(1500);                                          // still alive = USB keeps the board powered
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0);          // BOOT button wakes it
  esp_deep_sleep_start();
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
float vbusVolts() { return vbus; }
float coreTempC() { return temperatureRead(); }

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
    vbus = pmu.isVbusIn() ? pmu.getVbusVoltage() / 1000.f : 0;
  }
}

}  // namespace power
