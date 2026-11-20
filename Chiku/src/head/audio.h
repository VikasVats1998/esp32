// audio.h - speaker output through the ES8311 codec (I2S, 16 kHz).
// Beeps and melodies are synthesised on the fly (no sound files). A small task
// plays a queue of notes. The amplifier (PA pin) is only on while sound plays.
#pragma once
#include <Arduino.h>

namespace audio {
enum Melody : uint8_t {
  MEL_STARTUP, MEL_ALARM, MEL_HAPPY, MEL_SAD, MEL_PET, MEL_TIMER, MEL_DIZZY, MEL_CLICK, MEL_COUNT
};
bool init();                                   // codec + I2S + task
bool online();
void beep(uint16_t hz, uint16_t ms);           // queued
void play(Melody m, bool loop = false);        // loop = repeat until stop()
void stop();                                   // silence now
void setVolume(uint8_t percent);               // 0..100
bool playing();
}  // namespace audio
