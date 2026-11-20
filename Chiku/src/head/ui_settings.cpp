// ui_settings.cpp - brightness, volume, Wi-Fi info and quick actions.
#include "ui_screens.h"
#include "ui_common.h"
#include "storage.h"
#include "events.h"
#include "net.h"
#include "alarm_timer.h"
#include "audio.h"

namespace ui_settings {
static lv_obj_t *lBri, *lVol, *lNet;

static void changed() {
  storage::save();
  events::publish(events::EV_SETTINGS);
}
static void onBri(lv_event_t* e) {
  Settings& s = storage::get();
  int v = s.brightness + (int)(intptr_t)lv_event_get_user_data(e);
  s.brightness = constrain(v, 10, 255);
  changed();
}
static void onVol(lv_event_t* e) {
  Settings& s = storage::get();
  int v = s.volume + (int)(intptr_t)lv_event_get_user_data(e);
  s.volume = constrain(v, 0, 100);
  changed();
  audio::play(audio::MEL_CLICK);
}
static void onTestAlarm(lv_event_t*) { alarm_timer::testRing(); }

lv_obj_t* create(lv_obj_t* parent) {
  lv_obj_t* p = uic::panel(parent);
  uic::label(p, &lv_font_montserrat_20, 0xFFFFFF, "Settings", 50);
  lBri = uic::label(p, &lv_font_montserrat_20, 0xFFD060, "", 110);
  uic::button(p, "-", 100, 140, 80, 46, onBri, (void*)-20);
  uic::button(p, "+", 286, 140, 80, 46, onBri, (void*)20);
  lVol = uic::label(p, &lv_font_montserrat_20, 0x60D0FF, "", 205);
  uic::button(p, "-", 100, 235, 80, 46, onVol, (void*)-10);
  uic::button(p, "+", 286, 235, 80, 46, onVol, (void*)10);
  uic::button(p, "Test alarm", 163, 300, 140, 40, onTestAlarm);
  lNet = uic::label(p, &lv_font_montserrat_14, 0x80C0FF, "", 355);
  return p;
}

void update() {
  Settings& s = storage::get();
  char buf[64];
  snprintf(buf, sizeof(buf), "Brightness %d%%", s.brightness * 100 / 255);
  uic::setText(lBri, buf);
  snprintf(buf, sizeof(buf), "Volume %d%%", s.volume);
  uic::setText(lVol, buf);
  if (net::connected()) snprintf(buf, sizeof(buf), "%s   chiku.local", net::ip());
  else if (net::portalActive()) snprintf(buf, sizeof(buf), "Join Wi-Fi \"%s\" to set up", AP_SSID);
  else snprintf(buf, sizeof(buf), "Wi-Fi: not connected");
  uic::setText(lNet, buf);
}
}  // namespace ui_settings
