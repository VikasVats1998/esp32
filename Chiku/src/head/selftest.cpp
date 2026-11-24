// selftest.cpp - see selftest.h
#include "selftest.h"
#include "config.h"
#include "bus.h"
#include "display.h"
#include "touch.h"
#include "imu.h"
#include "rtc_time.h"
#include "audio.h"
#include "mic.h"
#include "servo.h"
#include "sonar.h"
#include "camlink.h"
#include "pads.h"
#include "events.h"
#include <lvgl.h>

namespace selftest {

static lv_obj_t* report = nullptr;
static char text[900] = "";

bool requested() {
  pinMode(PIN_BOOT, INPUT_PULLUP);
  delay(30);
  return digitalRead(PIN_BOOT) == LOW;
}

// Keep everything alive while we wait.
static void pump(uint32_t ms) {
  uint32_t t0 = millis();
  while (millis() - t0 < ms) {
    servo::update(20);
    camlink::update();
    sonar::update();
    imu::update();
    lv_timer_handler();
    delay(10);
  }
}

static void line(const char* name, bool pass, const char* detail = "") {
  char row[96];
  snprintf(row, sizeof(row), "%-8s %s %s\n", name, pass ? "PASS" : "FAIL", detail);
  Serial.print(row);
  strlcat(text, row, sizeof(text));
  lv_label_set_text(report, text);
  lv_timer_handler();
}

void run() {
  Serial.println("\n=== Chiku self-test ===");
  lv_obj_t* scr = lv_screen_active();
  lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
  report = lv_label_create(scr);
  lv_obj_set_style_text_font(report, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(report, lv_color_white(), 0);
  lv_obj_align(report, LV_ALIGN_TOP_LEFT, 90, 70);
  lv_label_set_text(report, "Self-test...");
  display::setBrightness(200);
  lv_timer_handler();
  char d[48];

  // 1. display: colour bars
  lv_obj_t* bar = lv_obj_create(scr);
  lv_obj_set_size(bar, 300, 20);
  lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, 0, -50);
  const uint32_t cols[] = {0xFF0000, 0x00FF00, 0x0000FF, 0xFFFFFF};
  for (uint32_t c : cols) { lv_obj_set_style_bg_color(bar, lv_color_hex(c), 0); pump(150); }
  line("display", true, "(bars shown)");

  // 2. touch: needs a touch within 6 s
  lv_label_set_text(report, "Touch the screen...");
  uint32_t t0 = millis(); bool touched = false;
  while (millis() - t0 < 6000 && !touched) { pump(20); touched = touch::isDown(); }
  text[0] = 0;
  line("touch", touched, touched ? "" : "(no touch in 6 s)");

  // 3. IMU
  float x = 0, y = 0, z = 0;
  pump(800);
  imu::getAccel(x, y, z);
  float mag = sqrtf(x * x + y * y + z * z);
  snprintf(d, sizeof(d), "%.2f g", mag);
  line("imu", imu::online() && mag > 0.8f && mag < 1.2f, d);

  // 4. RTC
  struct tm t;
  bool rtcOk = rtc_time::rtcPresent();
  if (rtc_time::localTime(t)) snprintf(d, sizeof(d), "%02d:%02d", t.tm_hour, t.tm_min); else d[0] = 0;
  line("rtc", rtcOk, d);

  // 5. speaker (cannot be measured: listen)
  audio::setVolume(50);
  audio::play(audio::MEL_STARTUP);
  pump(900);
  line("speaker", audio::online(), "(did you hear it?)");
  {
    char md[40];
    snprintf(md, sizeof(md), mic::online() ? "(%d dB now)" : "(ES7210 not found at 0x40)", mic::db());
    line("microphones", mic::online(), md);
  }

  // 6. servos: move each slowly +-15 deg
  bool pca = servo::online();
  if (pca) {
    for (uint8_t c = 0; c < SERVO_COUNT; c++) {
      servo::setTarget(c, 90, 40); pump(600);
      servo::setTarget(c, 105, 40); pump(600);
      servo::setTarget(c, 75, 40); pump(900);
      servo::setTarget(c, 90, 40); pump(500);
    }
  }
  line("pca9685", pca, pca ? "(5 servos moved)" : "(not found on GPIO17/18)");

  // 7/8. cam link and sonar (sonar data comes from the cam)
  t0 = millis();
  while (millis() - t0 < 4000 && !camlink::alive()) pump(50);
  line("camlink", camlink::alive(), camlink::alive() ? camlink::ip() : "(no reply)");
  pump(800);
  int cm = sonar::distanceCm();
  snprintf(d, sizeof(d), "%d cm", cm);
  line("sonar", cm > 0, d);

  snprintf(d, sizeof(d), "(touched now: 0x%02X)", pads::rawState());
  line("pads", pads::expanderOnline(), d);
  audio::say("hello", audio::BAB_HAPPY);
  pump(2500);
  line("voice", audio::hasClip("hello"), audio::hasClip("hello") ? "(clip played)" : "(no clip: babble)");

  strlcat(text, "\nDone. Touch to reboot.", sizeof(text));
  lv_label_set_text(report, text);
  Serial.println("=== self-test done ===");
  servo::releaseAll();
  for (;;) {
    pump(50);
    if (touch::isDown()) { delay(300); ESP.restart(); }
  }
}

}  // namespace selftest
