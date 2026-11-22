// storage.h - settings saved in NVS (small values) and LittleFS (web page).
// One plain struct holds everything; modules read it directly, and call
// storage::save() after changing it, then publish EV_SETTINGS.
#pragma once
#include <Arduino.h>
#include "config.h"

struct AlarmCfg {
  bool enabled;
  uint8_t hour, minute;
  uint8_t days;      // bit0=Mon ... bit6=Sun, 0x7F = every day
};

struct ServoCfg {
  int8_t trim;       // degrees added to every command
  uint8_t minDeg;    // soft limit
  uint8_t maxDeg;
  bool invert;
};

static const uint8_t MAX_ALARMS = 8;

struct Settings {
  char ssid[33];
  char pass[65];
  float lat, lon;
  uint8_t brightness;      // 0..255
  uint8_t volume;          // 0..100
  AlarmCfg alarms[MAX_ALARMS];
  ServoCfg servo[SERVO_COUNT];
  bool servoCalibrated;
  char serverUrl[64];      // hub server, e.g. http://192.168.1.20:8080 (empty = off)
  char serverToken[24];
  bool clapWake;           // wake / react on claps
  uint8_t clapSens;        // 0..100, higher = reacts to quieter claps
  bool wakeWord;           // voice wake word (when the engine is available)
  bool aiVoice;            // AI voice assistant (hook only, not implemented yet)
};

namespace storage {
void init();                 // mounts LittleFS, loads NVS
Settings& get();
void save();                 // writes everything to NVS
void clearWifi();
bool fsReady();
}  // namespace storage
