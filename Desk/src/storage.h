// storage.h - settings kept in NVS: Wi-Fi, paired clients (their keys!), small options.
#pragma once
#include <Arduino.h>
#include "desklink.h"

struct PairedClient {
  bool used;
  char id[9];                 // 8 hex chars, chosen by the desk at pairing
  char name[17];
  desklink::Key key;
};

struct Settings {
  char ssid[33];
  char pass[65];
  char pin[7];                // 6-digit OTA password, shown only in the log during the pairing window
  bool armed;                 // false: movement commands are refused (child lock)
  bool presetHold;            // true: the desk needs the memory key held during travel
  float estOffsetCm;          // height estimator calibration
  PairedClient clients[4];
};

namespace storage {
void init();
Settings& get();
void save();
void clearWifi();
}  // namespace storage
