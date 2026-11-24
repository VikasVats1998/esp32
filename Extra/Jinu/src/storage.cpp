// storage.cpp - see storage.h
#include "storage.h"
#include "logbuf.h"
#include <Preferences.h>
#include <LittleFS.h>

namespace storage {
static Settings S;
static Preferences prefs;
static bool fsOk = false;

void init() {
  fsOk = LittleFS.begin(true);
  memset(&S, 0, sizeof(S));
  S.brightness = BRIGHTNESS_DEFAULT;
  S.volume = 60;
  const uint8_t inv[SERVO_COUNT] = SERVO_INVERT_DEFAULT;
  for (int i = 0; i < SERVO_COUNT; i++) S.servo[i] = {0, 10, 170, (bool)inv[i]};
  prefs.begin("jinu", false);
  prefs.getString("ssid", S.ssid, sizeof(S.ssid));
  prefs.getString("pass", S.pass, sizeof(S.pass));
  S.brightness = prefs.getUChar("bri", S.brightness);
  S.volume = prefs.getUChar("vol", S.volume);
  if (prefs.getBytesLength("servo") == sizeof(S.servo)) prefs.getBytes("servo", S.servo, sizeof(S.servo));
}
Settings& get() { return S; }
void save() {
  LOGI("storage", "settings saved");
  prefs.putString("ssid", S.ssid);
  prefs.putString("pass", S.pass);
  prefs.putUChar("bri", S.brightness);
  prefs.putUChar("vol", S.volume);
  prefs.putBytes("servo", S.servo, sizeof(S.servo));
}
void clearWifi() { S.ssid[0] = 0; S.pass[0] = 0; save(); }
bool fsReady() { return fsOk; }
}  // namespace storage
