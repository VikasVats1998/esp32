// storage.h - everything the gimbal module remembers (NVS): pairing key, remote address, boot counter,
// joystick calibration, macro definitions.
#pragma once
#include <Arduino.h>
#include "desklink.h"
#include "remotelink.h"

struct Axis { uint16_t min, center, max; };          // PWM counts 0..1023 (0..3.3 V)
struct MacroStep { uint8_t key; uint16_t pressMs; uint16_t gapMs; };   // key = bit index of remotelink::KeyBit (0..5)
struct MacroDef { uint8_t count; MacroStep steps[6]; };

struct Settings {
  uint32_t magic;
  bool paired;
  uint8_t remoteMac[6];
  desklink::Key key;
  uint32_t session;                                  // boot counter of this module (frame session)
  uint32_t remoteSession;                            // newest session accepted from the remote (replay guard)
  Axis axis[3];                                      // x, y, zoom
  MacroDef macros[remotelink::MACRO_COUNT];
};

namespace storage {
void init();
Settings& get();
void save();
void factoryReset();
}  // namespace storage
