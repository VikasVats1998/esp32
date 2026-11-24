// main.cpp - Gimbal remote.
//   loop: read inputs -> send frames (50 Hz when something happens, 10 Hz when idle) -> LED / beeps -> sleep when idle
#include <Arduino.h>
#include <driver/rtc_io.h>
#include <esp_sleep.h>
#include "config.h"
#include "console.h"
#include "inputs.h"
#include "logbuf.h"
#include "rlink.h"
#include "storage.h"
#include "ui.h"

static void goToSleep() {
  LOGI("power", "idle for %lu s: deep sleep (any button wakes up)", SLEEP_AFTER_MS / 1000);
  rlink::sendNeutral();
  delay(30);
  uint64_t mask = 0;
  for (int p : {PIN_BTN_CAPTURE, PIN_BTN_ZOOM_IN, PIN_BTN_ZOOM_OUT, PIN_BTN_MODE, PIN_JOY_SW}) {
    mask |= 1ULL << p;
    rtc_gpio_init((gpio_num_t)p);
    rtc_gpio_set_direction((gpio_num_t)p, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pullup_en((gpio_num_t)p);
    rtc_gpio_pulldown_dis((gpio_num_t)p);
  }
#if PIN_STATUS_LED >= 0
  rgbLedWrite(PIN_STATUS_LED, 0, 0, 0);
#endif
  esp_sleep_enable_ext1_wakeup_io(mask, ESP_EXT1_WAKEUP_ANY_LOW);
  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);
  logbuf::init();
  storage::init();
  LOGI("boot", "gimbal remote fw %s, reset reason %d", FW_VERSION, (int)esp_reset_reason());
  ui::init();
  inputs::init();                                   // measures the stick centre
  rlink::init();
  console::init();
  ui::signalOk();
}

void loop() {
  inputs::Event ev = inputs::update();
  if (ev == inputs::EV_PAIR) { rlink::startPairing(); ui::beep(900, 120); }
  if (ev == inputs::EV_CALIBRATE) ui::beep(inputs::calibrating() ? 700 : 1500, 150);
  if (ev == inputs::EV_FINE_CHANGED) ui::beep(inputs::state().fine ? 600 : 1000, 100);

  const inputs::State& in = inputs::state();
  bool idle = !in.stickActive && in.keys == 0;
  rlink::update(in, idle);

  bool ok;
  if (rlink::pairingJustFinished(ok)) { if (ok) ui::signalOk(); else ui::signalError(); }

  static bool wasLinked = false;
  bool linked = rlink::linkOk();
  if (linked != wasLinked) { wasLinked = linked; if (rlink::paired()) { if (linked) ui::signalOk(); else ui::beep(400, 150); } }

  ui::update();
  console::update();

  if (millis() - in.lastInputMs > SLEEP_AFTER_MS && !rlink::pairing()) goToSleep();
  delay(4);
}
