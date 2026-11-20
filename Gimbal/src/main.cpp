// main.cpp - Gimbal module. The outputs are made safe (keys released, stick pins Hi-Z) before anything else.
#include <Arduino.h>
#include "config.h"
#include "console.h"
#include "logbuf.h"
#include "macros.h"
#include "outputs.h"
#include "rlink.h"
#include "storage.h"

static void statusLed() {
#if PIN_STATUS_LED >= 0
  static uint32_t last = 0;
  if (millis() - last < 100) return;
  last = millis();
  uint8_t r = 0, g = 0, b = 0;
  if (outputs::learning()) { r = 30; g = 30; }                                  // yellow: learning
  else if (rlink::pairingOpen()) { r = 20; b = 40; }                            // purple: pairing window
  else if (!rlink::linked()) {                                                  // waiting for the remote
    if (storage::get().paired) b = 20;                                          // blue: paired, remote not connected
    else { r = 25; b = 10; }                                                    // pink: not paired yet
  }
  else if (macros::running()) { g = 30; b = 30; }                               // cyan: macro
  else if (outputs::injecting()) { r = 40; g = 20; }                            // amber: the remote drives the sticks
  else g = 12;                                                                  // green: linked, idle
  rgbLedWrite(PIN_STATUS_LED, r, g, b);
#endif
}

void setup() {
  outputs::init();                                  // FIRST
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);
  logbuf::init();
  storage::init();
  LOGI("boot", "gimbal module fw %s, reset reason %d", FW_VERSION, (int)esp_reset_reason());
  macros::init();
  rlink::init();
  console::init();
}

void loop() {
  rlink::update();
  macros::update();
  outputs::update();
  console::update();
  statusLed();
  static uint32_t lastHeap = 0;
  if (millis() - lastHeap > 60000) { lastHeap = millis(); LOGI("heap", "free %lu KB", (unsigned long)(ESP.getFreeHeap() / 1024)); }
  delay(1);
}
