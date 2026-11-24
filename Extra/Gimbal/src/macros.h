// macros.h - sequences of button presses the module performs on its own (recenter, switch camera, ActiveTrack ...).
// Definitions are stored in flash and can be changed live over the USB console without re-flashing:
//     macro set 3 m:900:0         (key:press ms:pause after ms, comma separated, max 6 steps)
//     macro show      macro run 4      macro reset
#pragma once
#include <Arduino.h>

namespace macros {
void init();                              // loads from flash (or the defaults)
void start(uint8_t id);
void cancel();
void update();                            // sequencer, call often
bool running();
uint16_t keysMask();                      // keys the running macro presses right now
const char* name(uint8_t id);
bool set(uint8_t id, const char* spec, String& err);
String describe(uint8_t id);
void resetDefaults();
}  // namespace macros
