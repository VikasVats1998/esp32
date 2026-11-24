// mic.h - the two onboard microphones: ES7210 ADC (I2C 0x40 on the onboard bus) sends sound on the
// same I2S bus as the speaker codec. A small task measures the loudness. Used for: listening bars on
// the face, "loud sound -> surprised" (EV_LOUD), the Sound tile on the Sensors screen and the mic
// check in first-time setup. Not included: wake word / speech recognition (needs ESP-SR, see docs).
// While Chiku itself plays a sound the loud-sound event is suppressed (it would hear itself).
#pragma once
#include <Arduino.h>

namespace mic {
bool init();            // after audio::init()
bool online();
float level();          // 0..1 smoothed loudness
int db();               // rough sound level in dB (not calibrated), -1 if offline
uint32_t bursts();      // voice-like sounds heard so far (loud for 0.25-3 s): setup mic check
}  // namespace mic
