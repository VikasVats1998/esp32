// main.cpp - starts every module and creates the FreeRTOS tasks.
//   core 1: UI task (LVGL, face, touch, reactions)              prio 3
//   core 0: actuator task (motion + servos, 50 Hz)               prio 4
//           sensor task (IMU, sonar, cam link, TTP223, alarms)   prio 3
//           audio task (prio 2) and network task (prio 1) live in their modules
#include <Arduino.h>
#include <esp_task_wdt.h>
#include "config.h"
#include "events.h"
#include "bus.h"
#include "storage.h"
#include "display.h"
#include "touch.h"
#include "face.h"
#include "ui.h"
#include "behavior.h"
#include "servo.h"
#include "motion.h"
#include "imu.h"
#include "sonar.h"
#include "camlink.h"
#include "audio.h"
#include "power.h"
#include "rtc_time.h"
#include "alarm_timer.h"
#include "weather.h"
#include "net.h"
#include "selftest.h"

static QueueHandle_t uiQueue;

static void uiTask(void*) {
  esp_task_wdt_add(nullptr);
  uint32_t last = millis(), lastPower = 0, lastFace = 0;
  for (;;) {
    uint32_t now = millis();
    // 1. events for the UI and behaviour
    events::Event e;
    while (xQueueReceive(uiQueue, &e, 0) == pdTRUE) {
      switch (e.type) {      // anything the user does counts as activity
        case events::EV_TOUCH_TAP: case events::EV_TOUCH_LONG: case events::EV_SWIPE:
        case events::EV_SHAKE: case events::EV_PICKUP: case events::EV_PET:
        case events::EV_TAP_IMU: case events::EV_ALARM_RING:
          power::activity();
          break;
        default: break;
      }
      ui::onEvent(e);
      behavior::onEvent(e);
    }
    // 2. power state
    if (now - lastPower > 200) { lastPower = now; power::update(); }
    // 3. animation at FACE_FPS
    if (now - lastFace >= 1000 / FACE_FPS) {
      uint32_t dt = now - lastFace;
      lastFace = now;
      ui::update(dt);
      behavior::update(dt);
    }
    lv_timer_handler();
    esp_task_wdt_reset();
    vTaskDelay(pdMS_TO_TICKS(5));
    (void)last;
  }
}

static void actuatorTask(void*) {
  esp_task_wdt_add(nullptr);
  // Soft start: wait for power to settle, then move servos home one by one
  vTaskDelay(pdMS_TO_TICKS(800));
  const uint8_t order[SERVO_COUNT] = {SERVO_HEAD_PAN, SERVO_LEFT_HIP, SERVO_RIGHT_HIP,
                                      SERVO_LEFT_ANKLE, SERVO_RIGHT_ANKLE};
  for (uint8_t c : order) { servo::setTarget(c, 90); vTaskDelay(pdMS_TO_TICKS(SERVO_STAGGER_MS)); }
  TickType_t wake = xTaskGetTickCount();
  uint32_t last = millis();
  for (;;) {
    uint32_t now = millis();
    uint32_t dt = now - last;
    last = now;
    motion::update(dt);
    servo::update(dt);
    esp_task_wdt_reset();
    vTaskDelayUntil(&wake, pdMS_TO_TICKS(20));
  }
}

static void sensorTask(void*) {
  esp_task_wdt_add(nullptr);
  uint32_t lastSlow = 0;
  for (;;) {
    uint32_t now = millis();
    imu::update();
    touch::pollPet();
    camlink::update();
    sonar::update();
    if (now - lastSlow >= 200) {
      lastSlow = now;
      alarm_timer::update();
      rtc_time::update();
    }
    esp_task_wdt_reset();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void setup() {
  Serial.begin(115200);
  bool test = FEATURE_SELFTEST && selftest::requested();
  events::init();
  bus::init();
  storage::init();
  power::init();
  rtc_time::init();

  if (!display::init()) { Serial.println("display init failed"); }
  if (!touch::init()) Serial.println("touch init failed");
  ui::init();
  behavior::init();

  uiQueue = events::subscribe(32);
  motion::init();
  sonar::init();
#if FEATURE_IMU
  if (!imu::init()) Serial.println("IMU not found");
#endif
#if FEATURE_CAMLINK
  camlink::init();
#endif
#if FEATURE_AUDIO
  if (!audio::init()) Serial.println("audio init failed");
#endif
#if FEATURE_SERVOS
  if (!servo::init()) Serial.println("PCA9685 not found on GPIO17/18");
#endif
  alarm_timer::init();
  weather::init();

  if (test) selftest::run();            // never returns

  ui::applyBrightness();
  audio::play(audio::MEL_STARTUP);
  xTaskCreatePinnedToCore(uiTask, "ui", 12288, nullptr, 3, nullptr, 1);
  xTaskCreatePinnedToCore(actuatorTask, "actuator", 4096, nullptr, 4, nullptr, 0);
  xTaskCreatePinnedToCore(sensorTask, "sensors", 6144, nullptr, 3, nullptr, 0);
#if FEATURE_NET
  net::init();
#endif
}

void loop() {
#if DEBUG_HEAP_LOG
  Serial.printf("[heap] free %u KB, PSRAM free %u KB, min heap %u KB\n", ESP.getFreeHeap() / 1024,
                ESP.getFreePsram() / 1024, ESP.getMinFreeHeap() / 1024);
#endif
  vTaskDelay(pdMS_TO_TICKS(60000));
}
