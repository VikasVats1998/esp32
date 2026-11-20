// power.cpp - see power.h
#include "power.h"
#include "config.h"
#include "events.h"

namespace power {
static State st = ACTIVE;
static volatile uint32_t lastActivity = 0;
static volatile bool awake = false;

static void setState(State s) {
  if (s == st) return;
  st = s;
  setCpuFrequencyMhz(s == SLEEP ? 80 : 160);       // slower CPU while asleep
  events::publish(events::EV_POWER, s);
}

void init() { lastActivity = millis(); setCpuFrequencyMhz(160); }
State state() { return st; }

void activity() {
  lastActivity = millis();
  if (st != ACTIVE) { setState(ACTIVE); events::publish(events::EV_WAKE); }
}

void keepAwake(bool on) { awake = on; if (on) activity(); }

void update() {
  if (awake) return;
  uint32_t idle = millis() - lastActivity;
  if (idle > SLEEP_AFTER_MS) setState(SLEEP);
  else if (idle > DIM_AFTER_MS) setState(DIM);
}
}  // namespace power
