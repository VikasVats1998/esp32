// storage.h - settings in NVS + LittleFS (web page, voice clips).
#pragma once
#include <Arduino.h>
#include "config.h"

struct ServoCfg { int8_t trim; uint8_t minDeg, maxDeg; bool invert; };

struct Settings {
  char ssid[33];
  char pass[65];
  uint8_t brightness;     // 0..255
  uint8_t volume;         // 0..100
  ServoCfg servo[SERVO_COUNT];
};

namespace storage {
void init();            // mounts LittleFS, loads NVS
Settings& get();
void save();
void clearWifi();
bool fsReady();
}  // namespace storage
