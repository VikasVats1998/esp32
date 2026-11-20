// storage.h - what the remote remembers (NVS): pairing key, gimbal address, boot counter, joystick calibration.
#pragma once
#include <Arduino.h>
#include "desklink.h"

struct StickCal { uint16_t min, max; };              // raw 12 bit ADC values at the end stops

struct Settings {
  uint32_t magic;
  bool paired;
  uint8_t gimbalMac[6];
  desklink::Key key;
  uint32_t session;                                  // boot counter: part of every frame (replay protection)
  uint32_t gimbalSession;                            // newest session seen from the gimbal (replay guard for its status frames)
  StickCal cal[2];                                   // x, y
  bool fine;                                         // fine mode (scaled down stick) is on
};

namespace storage {
void init();
Settings& get();
void save();
void factoryReset();
}  // namespace storage
