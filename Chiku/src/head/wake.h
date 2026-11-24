// wake.h - voice wake word + spoken commands (Espressif ESP-SR, runs fully on the board).
// Say the wake word ("Hi ESP"), the robot looks up and listens, then say one command.
// Events: EV_WAKEWORD, then EV_VOICE a=Cmd, or EV_VOICE_END after ~6 s of silence.
#pragma once
#include <Arduino.h>

namespace wake {
enum Cmd : uint8_t { C_LIGHT_ON, C_LIGHT_OFF, C_FAN_ON, C_FAN_OFF, C_SLEEP, C_HAPPY, C_DANCE, C_BRIGHTER, C_DIMMER };
void init();              // after mic::init(); needs the "model" partition (see docs)
bool running();           // engine started
const char* error();      // why it did not start ("" if running)
}  // namespace wake
