// main.cpp - starts every module and the FreeRTOS tasks (the C6 has ONE core).
//   actuator task (prio 4): gaits + servos, 50 Hz
//   face task     (prio 3): face drawing 30 fps, reactions, power state, RGB LED
//   sensor task   (prio 3): touch pads, sonar, BOOT button
//   audio task    (prio 2) and network task (prio 1) live in their modules
#include <Arduino.h>
#include <esp_task_wdt.h>
#include "config.h"
#include "logbuf.h"
#include "events.h"
#include "storage.h"
#include "display.h"
#include "face.h"
#include "behavior.h"
#include "servo.h"
#include "motion.h"
#include "pads.h"
#include "sonar.h"
#include "audio.h"
#include "power.h"
#include "net.h"
#include "selftest.h"
#include "diag.h"

static QueueHandle_t faceQueue;

static void faceTask(void*) {
  esp_task_wdt_add(nullptr);
  uint32_t lastFrame = millis(), lastPower = 0, lastLed = 0;
  for (;;) {
    uint32_t now = millis();
    events::Event e;
    while (xQueueReceive(faceQueue, &e, 0) == pdTRUE) behavior::onEvent(e);
    if (now - lastPower > 200) { lastPower = now; power::update(); }
    uint32_t dt = now - lastFrame;
    if (dt >= 1000 / FACE_FPS) {
      lastFrame = now;
      face::update(dt);
      behavior::update(dt);
      if (MOOD_LED_LEVEL && now - lastLed > 100) {            // RGB LED glows in the mood colour
        lastLed = now;
        uint32_t c = face::ledColor();
        float k = power::state() == power::SLEEP ? 0.f : MOOD_LED_LEVEL / 255.f;
        rgbLedWrite(PIN_RGB_LED, (uint8_t)(((c >> 16) & 255) * k), (uint8_t)(((c >> 8) & 255) * k), (uint8_t)((c & 255) * k));
      }
    }
    esp_task_wdt_reset();
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

static void actuatorTask(void*) {
  esp_task_wdt_add(nullptr);
  vTaskDelay(pdMS_TO_TICKS(800));                             // let the servo supply settle
  for (uint8_t c = 0; c < SERVO_COUNT; c++) { servo::setTarget(c, 90); vTaskDelay(pdMS_TO_TICKS(SERVO_STAGGER_MS)); }
  TickType_t wake = xTaskGetTickCount();
  uint32_t last = millis();
  for (;;) {
    uint32_t now = millis();
    motion::update(now - last);
    servo::update(now - last);
    last = now;
    esp_task_wdt_reset();
    vTaskDelayUntil(&wake, pdMS_TO_TICKS(20));
  }
}

static void sensorTask(void*) {
  esp_task_wdt_add(nullptr);
  bool wasDown = false;
  uint32_t downAt = 0;
  bool longSent = false;
  for (;;) {
    pads::update();
    sonar::update();
    bool down = digitalRead(PIN_BOOT) == LOW;                 // BOOT: short = explore mode, long = sleep
    uint32_t now = millis();
    if (down && !wasDown) { downAt = now; longSent = false; }
    if (down && !longSent && now - downAt > 900) { longSent = true; events::publish(events::EV_BUTTON, 2); }
    if (!down && wasDown && !longSent && now - downAt > 40) events::publish(events::EV_BUTTON, 1);
    wasDown = down;
    esp_task_wdt_reset();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void setup() {
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);                 // never block when no USB host is connected
  logbuf::init();
  LOGI("boot", "%s fw %s, reset reason %d, heap %lu KB", ROBOT_NAME, FW_VERSION, (int)esp_reset_reason(), (unsigned long)(ESP.getFreeHeap() / 1024));
  bool test = selftest::requested();
  events::init();
  storage::init();
  diag::set("filesystem", storage::fsReady(), "LittleFS");
  power::init();
  diag::set("display", display::init(), "ST7789 320x172");
  face::init();
  faceQueue = events::subscribe(32);
  motion::init();
  sonar::init();
  diag::set("pad_expander", pads::init(), "PCF8574 0x20");
  diag::set("audio", audio::init(), "I2S + MAX98357A");
  servo::init();
  if (test) selftest::run();                                  // never returns
  behavior::init();
  audio::play(audio::MEL_STARTUP);
  audio::say("hello", audio::BAB_HAPPY);
  xTaskCreate(actuatorTask, "actuator", 3072, nullptr, 4, nullptr);
  xTaskCreate(faceTask, "face", 6144, nullptr, 3, nullptr);
  xTaskCreate(sensorTask, "sensors", 3072, nullptr, 3, nullptr);
  net::init();
}

void loop() {
#if DEBUG_HEAP_LOG
  LOGI("heap", "free %lu KB, min %lu KB", (unsigned long)(ESP.getFreeHeap() / 1024), (unsigned long)(ESP.getMinFreeHeap() / 1024));
#endif
  vTaskDelay(pdMS_TO_TICKS(60000));
}
