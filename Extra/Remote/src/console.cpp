// console.cpp - see console.h
#include "console.h"
#include "config.h"
#include "inputs.h"
#include "rlink.h"
#include "storage.h"
#include "ui.h"
#include <Arduino.h>

namespace console {
static String line;

void init() { Serial.println("remote: status | pair | unpair | calibrate | fine | factory"); }

static void handle(String c) {
  c.trim();
  if (c == "status") {
    const inputs::State& s = inputs::state();
    Serial.printf("fw %s, %s, link %s, gimbal rssi %d, battery %d mV (%d%%), fine %d\n", FW_VERSION, rlink::paired() ? "paired" : "NOT paired", rlink::linkOk() ? "ok" : "down",
                  rlink::rssi(), ui::batteryMv(), ui::batteryPercent(), s.fine);
    Serial.printf("stick %d %d, keys 0x%02X, macro count %u\n", s.jx, s.jy, s.keys, s.macroCount);
    for (int a = 0; a < 2; a++) Serial.printf("axis %d calibrated %u..%u\n", a, storage::get().cal[a].min, storage::get().cal[a].max);
  } else if (c == "pair") rlink::startPairing();
  else if (c == "unpair") rlink::unpair();
  else if (c == "calibrate") inputs::startCalibration();
  else if (c == "fine") { storage::get().fine = !storage::get().fine; storage::save(); Serial.println("fine mode toggled (applies after restart)"); }
  else if (c == "factory") { storage::factoryReset(); Serial.println("erased"); }
  else if (c.length()) Serial.println("unknown command");
}

void update() {
  while (Serial.available()) {
    char ch = Serial.read();
    if (ch == '\n' || ch == '\r') { handle(line); line = ""; }
    else if (line.length() < 60) line += ch;
  }
}
}  // namespace console
