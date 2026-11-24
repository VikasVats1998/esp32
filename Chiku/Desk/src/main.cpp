// main.cpp - Desk module. Order matters: the button outputs are made safe (LOW) before anything else.
//   control task (core 1, prio 4): button timers + safety limits, height frames   every 10 ms
//   net task     (core 0, prio 1): Wi-Fi, secure link server, OTA (pairing window only)
#include <Arduino.h>
#include <esp_task_wdt.h>
#include "buttons.h"
#include "config.h"
#include "height.h"
#include "link.h"
#include "logbuf.h"
#include "net.h"
#include "storage.h"

static void controlTask(void*) {
  esp_task_wdt_add(nullptr);
  TickType_t wake = xTaskGetTickCount();
  uint32_t lastMoveSeen = 0;
  bool sawMotion = false;
  for (;;) {
    buttons::update();
    height::update();
    // A preset travel is finished when the height was changing and then stays still for 1.5 s.
    if (buttons::travelling()) {
      height::Info h = height::get();
      uint32_t now = millis();
      if (h.moving) { lastMoveSeen = now; sawMotion = true; }
      else if (sawMotion && h.src[0] == 'd' && now - lastMoveSeen > 1500) { buttons::travelDone(); sawMotion = false; }
    } else sawMotion = false;
    esp_task_wdt_reset();
    vTaskDelayUntil(&wake, pdMS_TO_TICKS(10));
  }
}

static void statusLed() {
#if PIN_STATUS_LED >= 0
  static uint32_t last = 0;
  if (millis() - last < 200) return;
  last = millis();
  uint8_t r = 0, g = 0, b = 0;
  if (buttons::moveDir() || buttons::travelling()) { r = 40; g = 20; }                 // amber: moving
  else if (desklink_server::lockedOut()) { r = 40; }                                  // red: locked out
  else if (desklink_server::pairingOpen()) { r = 20; b = 40; }                        // purple: pairing window
  else if (net::connected()) { g = 8; }                                               // dim green: ready
  else { b = 20; }                                                                    // blue: no network
  rgbLedWrite(PIN_STATUS_LED, r, g, b);
#endif
}

void setup() {
  buttons::init();                                    // FIRST: all outputs LOW
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);
  logbuf::init();
  storage::init();
  LOGI("boot", "desk module fw %s, reset reason %d, heap %lu KB", FW_VERSION, (int)esp_reset_reason(), (unsigned long)(ESP.getFreeHeap() / 1024));
  LOGI("boot", "device PIN %s (needed for OTA and for changing Wi-Fi during the pairing window)", storage::get().pin);
  height::init();
  xTaskCreatePinnedToCore(controlTask, "control", 4096, nullptr, 4, nullptr, 1);
  net::init();
}

void loop() {
  statusLed();
  static uint32_t lastHeap = 0, lastSaved = 0;
  if (millis() - lastHeap > 60000) {
    lastHeap = millis();
    LOGI("heap", "free %lu KB, min %lu KB, clients %u", (unsigned long)(ESP.getFreeHeap() / 1024), (unsigned long)(ESP.getMinFreeHeap() / 1024), (unsigned)desklink_server::clientCount());
  }
  // remember the estimated height 3 s after the last button activity
  if (millis() - buttons::lastActivityMs() > 3000 && millis() - lastSaved > 10000 && fabsf(storage::get().estOffsetCm - height::estimate()) > 0.5f) {
    lastSaved = millis();
    storage::get().estOffsetCm = height::estimate();
    storage::save();
  }
  delay(50);
}
