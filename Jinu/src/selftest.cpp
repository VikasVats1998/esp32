// selftest.cpp - see selftest.h
#include "selftest.h"
#include "config.h"
#include "display.h"
#include "servo.h"
#include "pads.h"
#include "sonar.h"
#include "audio.h"

namespace selftest {
static int row = 0;

bool requested() {
  pinMode(PIN_BOOT, INPUT_PULLUP);
  delay(30);
  return digitalRead(PIN_BOOT) == LOW;
}

static void pump(uint32_t ms) {                 // keep servos / sensors alive while waiting
  uint32_t t0 = millis();
  while (millis() - t0 < ms) { servo::update(20); pads::update(); sonar::update(); delay(10); }
}

static void line(const char* name, bool pass, const char* detail = "") {
  Arduino_GFX* g = display::canvas();
  char buf[64];
  snprintf(buf, sizeof(buf), "%-8s %s %s", name, pass ? "PASS" : "FAIL", detail);
  Serial.println(buf);
  g->setTextSize(2);
  g->setTextColor(pass ? 0x07E0 : 0xF800);
  g->setCursor(6, 6 + row * 22);
  g->print(buf);
  display::flush();
  row++;
}

void run() {
  Serial.println("\n=== Jinu self-test ===");
  Arduino_GFX* g = display::canvas();
  display::setBrightness(200);
  const uint16_t cols[] = {0xF800, 0x07E0, 0x001F, 0xFFFF, 0x0000};
  for (uint16_t c : cols) { g->fillScreen(c); display::flush(); delay(250); }
  g->fillScreen(0);
  line("display", true, "(colours shown)");
  char d[40];

  servo::init();
  for (uint8_t c = 0; c < SERVO_COUNT; c++) {                 // each servo +-15 degrees, slowly
    servo::setTarget(c, 90, 40); pump(600);
    servo::setTarget(c, 105, 40); pump(600);
    servo::setTarget(c, 75, 40); pump(900);
    servo::setTarget(c, 90, 40); pump(500);
  }
  servo::releaseAll();
  line("servos", true, "(4 moved - did they?)");

  bool pcf = pads::online();
  snprintf(d, sizeof(d), "raw 0x%02X", pads::rawState());
  line("pads", pcf, pcf ? d : "(PCF8574 missing)");
  line("touch", true, "touch each pad:");
  uint8_t seen = 0;
  uint32_t t0 = millis();
  while (millis() - t0 < 8000 && seen != 0x1F) { pump(20); seen |= pads::rawState(); }
  snprintf(d, sizeof(d), "%d/5 pads", __builtin_popcount(seen));
  line("pads", seen == 0x1F, d);

  pump(800);
  int cm = sonar::distanceCm();
  snprintf(d, sizeof(d), "%d cm", cm);
  line("sonar", cm > 0, d);

  audio::setVolume(50);
  audio::play(audio::MEL_STARTUP);
  pump(900);
  line("speaker", audio::online(), "(heard it?)");
  audio::say("hello", audio::BAB_HAPPY);
  pump(2500);
  line("voice", audio::hasClip("hello"), audio::hasClip("hello") ? "(clip)" : "(babble)");
  g->setTextColor(0xFFFF);
  g->setCursor(6, 6 + row * 22);
  g->print("Done. Press BOOT.");
  display::flush();
  Serial.println("=== done ===");
  for (;;) { pump(30); if (digitalRead(PIN_BOOT) == LOW) { delay(300); ESP.restart(); } }
}
}  // namespace selftest
