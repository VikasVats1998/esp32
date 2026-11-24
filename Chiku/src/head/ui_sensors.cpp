// ui_sensors.cpp - all readings at once (UI kit "Sensors"): distance, camera, IMU, sound, battery,
// Wi-Fi. Tap Distance -> radar, Camera -> camera, Wi-Fi -> network.
#include "ui_screens.h"
#include "ui_common.h"
#include "ui.h"
#include "sonar.h"
#include "camlink.h"
#include "imu.h"
#include "mic.h"
#include "power.h"
#include "net.h"

namespace ui_sensors {
enum { T_DIST, T_CAM, T_IMU, T_SOUND, T_BATT, T_WIFI, T_COUNT };
static lv_obj_t* val[T_COUNT];

static void onTile(lv_event_t* e) {
  int t = (int)(intptr_t)lv_event_get_user_data(e);
  if (t == T_DIST) ui::show(ui::S_SONAR);
  else if (t == T_CAM) ui::show(ui::S_CAMERA);
  else if (t == T_WIFI) ui::show(ui::S_NETWORK);
}

lv_obj_t* create(lv_obj_t* parent) {
  lv_obj_t* p = uic::panel(parent);
  uic::title(p, "Sensors", 58);
  static const char* ICON[T_COUNT] = {LV_SYMBOL_GPS, LV_SYMBOL_IMAGE, LV_SYMBOL_REFRESH, LV_SYMBOL_AUDIO, LV_SYMBOL_BATTERY_FULL, LV_SYMBOL_WIFI};
  static const char* NAME[T_COUNT] = {"Distance", "Camera", "IMU", "Sound", "Battery", "Wi-Fi"};
  static const uint32_t COL[T_COUNT] = {uic::C_YELLOW, uic::C_PINK, uic::C_GREEN, uic::C_PURPLE, uic::C_ORANGE, uic::C_CYAN};
  for (int i = 0; i < T_COUNT; i++) {
    int x = 73 + (i % 2) * 164, y = 100 + (i / 2) * 92;
    bool link = i == T_DIST || i == T_CAM || i == T_WIFI;
    lv_obj_t* c = uic::card(p, x, y, 156, 84, link ? onTile : nullptr, (void*)(intptr_t)i);
    uic::label(c, &inter_14, COL[i], ICON[i], 12, LV_ALIGN_TOP_LEFT, 16);
    uic::label(c, &inter_14, uic::C_MUTED, NAME[i], 12, LV_ALIGN_TOP_LEFT, 40);
    val[i] = uic::label(c, &inter_20, uic::C_TEXT, "--", 40, LV_ALIGN_TOP_LEFT, 16);
  }
  return p;
}

void update(uint32_t) {
  char b[24];
  int cm = sonar::distanceCm();
  if (cm >= 0) snprintf(b, sizeof(b), "%d cm", cm); else strlcpy(b, "no echo", sizeof(b));
  uic::setText(val[T_DIST], b);
  uic::setText(val[T_CAM], camlink::alive() ? (strcmp(camlink::camState(), "on") ? "Asleep" : "Online") : "Offline");
  if (imu::online()) { float tl = imu::tiltDeg(); if (tl < 8) strlcpy(b, "Level", sizeof(b)); else snprintf(b, sizeof(b), "Tilt %.0f\xC2\xB0", tl); }
  else strlcpy(b, "Offline", sizeof(b));
  uic::setText(val[T_IMU], b);
  if (mic::online()) snprintf(b, sizeof(b), "%d dB", mic::db()); else strlcpy(b, "No mic", sizeof(b));
  uic::setText(val[T_SOUND], b);
  int pct = power::batteryPercent();
  if (pct < 0) strlcpy(b, "USB", sizeof(b));
  else snprintf(b, sizeof(b), "%d%%%s", pct, power::charging() ? " " LV_SYMBOL_CHARGE : "");
  uic::setText(val[T_BATT], b);
  if (net::connected()) snprintf(b, sizeof(b), "%d dBm", net::rssi()); else strlcpy(b, "Offline", sizeof(b));
  uic::setText(val[T_WIFI], b);
}
void onShow() {}
void onHide() {}
}  // namespace ui_sensors
