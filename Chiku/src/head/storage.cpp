// storage.cpp - see storage.h
#include "storage.h"
#include "logbuf.h"
#include <Preferences.h>
#include <LittleFS.h>

namespace storage {

static Settings S;
static Preferences prefs;
static bool fsOk = false;

static void setDefaults() {
  memset(&S, 0, sizeof(S));
  S.lat = WEATHER_LAT_DEFAULT;
  S.lon = WEATHER_LON_DEFAULT;
  S.brightness = BRIGHTNESS_DEFAULT;
  S.volume = 60;
  const uint8_t inv[SERVO_COUNT] = SERVO_INVERT_DEFAULT;
  for (int i = 0; i < SERVO_COUNT; i++) {
    S.servo[i] = {0, 10, 170, (bool)inv[i]};
  }
  S.servo[SERVO_HEAD_PAN] = {0, 0, 180, false};
  for (auto& a : S.alarms) { a.days = 0x7F; a.hour = 7; a.minute = 0; }
}

void init() {
  fsOk = LittleFS.begin(true);
  setDefaults();
  prefs.begin("chiku", false);
  prefs.getString("ssid", S.ssid, sizeof(S.ssid));
  prefs.getString("pass", S.pass, sizeof(S.pass));
  S.lat = prefs.getFloat("lat", S.lat);
  S.lon = prefs.getFloat("lon", S.lon);
  S.brightness = prefs.getUChar("bri", S.brightness);
  S.volume = prefs.getUChar("vol", S.volume);
  S.servoCalibrated = prefs.getBool("cal", false);
  if (prefs.getBytesLength("alarms") == sizeof(S.alarms)) prefs.getBytes("alarms", S.alarms, sizeof(S.alarms));
  if (prefs.getBytesLength("servo") == sizeof(S.servo)) prefs.getBytes("servo", S.servo, sizeof(S.servo));
}

Settings& get() { return S; }

void save() {
  LOGI("storage", "settings saved");
  prefs.putString("ssid", S.ssid);
  prefs.putString("pass", S.pass);
  prefs.putFloat("lat", S.lat);
  prefs.putFloat("lon", S.lon);
  prefs.putUChar("bri", S.brightness);
  prefs.putUChar("vol", S.volume);
  prefs.putBool("cal", S.servoCalibrated);
  prefs.putBytes("alarms", S.alarms, sizeof(S.alarms));
  prefs.putBytes("servo", S.servo, sizeof(S.servo));
}

void clearWifi() {
  S.ssid[0] = 0;
  S.pass[0] = 0;
  save();
}

bool fsReady() { return fsOk; }

}  // namespace storage
