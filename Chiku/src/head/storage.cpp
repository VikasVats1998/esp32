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
  S.screenRot = ROTATION_DEFAULT;
  strcpy(S.serverToken, "chiku");
  S.clapWake = true;
  S.clapSens = 60;
  S.wakeWord = true;
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
  S.setupDone = prefs.getBool("setup", false);
  S.screenRot = prefs.getShort("rot", S.screenRot);
  prefs.getString("srv", S.serverUrl, sizeof(S.serverUrl));
  if (prefs.isKey("srvtok")) prefs.getString("srvtok", S.serverToken, sizeof(S.serverToken));
  S.aiVoice = prefs.getBool("ai", false);
  S.clapWake = prefs.getBool("clap", S.clapWake);
  S.clapSens = prefs.getUChar("clapS", S.clapSens);
  S.wakeWord = prefs.getBool("wakeW", S.wakeWord);
  if (prefs.getBytesLength("alarms") == sizeof(S.alarms)) prefs.getBytes("alarms", S.alarms, sizeof(S.alarms));
  if (prefs.getUChar("srvmap", 0) < 2) {            // servo map changed (PCA9685 channels 1-5): start neutral, no old trims
    prefs.remove("servo");
    prefs.putUChar("srvmap", 2);
  }
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
  prefs.putBool("setup", S.setupDone);
  prefs.putShort("rot", S.screenRot);
  prefs.putString("srv", S.serverUrl);
  prefs.putString("srvtok", S.serverToken);
  prefs.putBool("ai", S.aiVoice);
  prefs.putBool("clap", S.clapWake);
  prefs.putUChar("clapS", S.clapSens);
  prefs.putBool("wakeW", S.wakeWord);
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
