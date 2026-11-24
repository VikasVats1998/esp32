// ui_settings.cpp - settings list (UI kit "Settings"): tap a row to change it or open it.
// Swipe right to go back (the list scrolls up / down).
#include "ui_screens.h"
#include "ui_common.h"
#include "ui.h"
#include "storage.h"
#include "events.h"
#include "net.h"
#include "alarm_timer.h"
#include "audio.h"
#include "behavior.h"
#include "display.h"

namespace ui_settings {
enum Row { R_BRI, R_VOL, R_MODE, R_SCREEN, R_SERVO, R_NET, R_DEMO, R_ALARM, R_SETUP, R_WIFI_RESET, R_FW, R_COUNT };
static const char* NAMES[R_COUNT] = {"Brightness", "Volume", "Mode", "Screen orientation", "Servo calibration",
                                     "Wi-Fi", "Mood demo", "Test alarm", "First-time setup", "Forget Wi-Fi", "Firmware"};
static lv_obj_t* val[R_COUNT];
static uint32_t resetArmed = 0;

static void changed() { storage::save(); events::publish(events::EV_SETTINGS); }

static void onRow(lv_event_t* e) {
  Settings& s = storage::get();
  switch ((Row)(intptr_t)lv_event_get_user_data(e)) {
    case R_BRI: { int p = (s.brightness * 100 + 127) / 255; p = p >= 100 ? 20 : ((p / 20) + 1) * 20; s.brightness = p * 255 / 100; changed(); break; }
    case R_VOL: s.volume = s.volume >= 100 ? 0 : ((s.volume / 20) + 1) * 20; changed(); audio::play(audio::MEL_CLICK); break;
    case R_MODE: behavior::setMode((behavior::Mode)((behavior::mode() + 1) % 3)); break;
    case R_SCREEN: ui::show(ui::S_ORIENT); break;
    case R_SERVO: ui::show(ui::S_CALIB); break;
    case R_NET: ui::show(ui::S_NETWORK); break;
    case R_DEMO: ui::startMoodDemo(); break;
    case R_ALARM: alarm_timer::testRing(); break;
    case R_SETUP: s.setupDone = false; storage::save(); ui::show(ui::S_SETUP); break;
    case R_WIFI_RESET:
      if (resetArmed && millis() - resetArmed < 4000) { resetArmed = 0; storage::clearWifi(); net::restartWifi(); ui::toast("Wi-Fi forgotten"); }
      else { resetArmed = millis(); ui::toast("Tap again to forget Wi-Fi"); }
      break;
    default: break;
  }
}

lv_obj_t* create(lv_obj_t* parent) {
  lv_obj_t* p = uic::panel(parent);
  uic::title(p, "Settings", 58);
  lv_obj_t* l = uic::list(p, 96, 320, 320);
  for (int i = 0; i < R_COUNT; i++) {
    lv_obj_t* r = uic::card(l, 0, 0, 320, 46, i == R_FW ? nullptr : onRow, (void*)(intptr_t)i, 0x1E232A, 23);
    uic::label(r, &inter_16, i == R_WIFI_RESET ? uic::C_RED : uic::C_TEXT, NAMES[i], 0, LV_ALIGN_LEFT_MID, 18);
    val[i] = uic::label(r, &inter_16, uic::C_CYAN, "", 0, LV_ALIGN_RIGHT_MID, -18);
  }
  return p;
}

void update(uint32_t) {
  Settings& s = storage::get();
  char b[40];
  snprintf(b, sizeof(b), "%d%%", (s.brightness * 100 + 127) / 255); uic::setText(val[R_BRI], b);
  snprintf(b, sizeof(b), "%d%%", s.volume); uic::setText(val[R_VOL], b);
  uic::setText(val[R_MODE], behavior::mode() == behavior::MODE_AUTO ? "Playful" : behavior::mode() == behavior::MODE_EXPLORE ? "Explore" : "Manual");
  snprintf(b, sizeof(b), "%d\xC2\xB0", display::rotation()); uic::setText(val[R_SCREEN], b);
  uic::setText(val[R_SERVO], s.servoCalibrated ? "Done" : "Not yet");
  uic::setText(val[R_NET], net::connected() ? s.ssid : "Offline");
  uic::setText(val[R_DEMO], LV_SYMBOL_PLAY);
  uic::setText(val[R_ALARM], LV_SYMBOL_BELL);
  uic::setText(val[R_SETUP], LV_SYMBOL_RIGHT);
  uic::setText(val[R_FW], FW_VERSION);
}
void onShow() {}
void onHide() {}
}  // namespace ui_settings
