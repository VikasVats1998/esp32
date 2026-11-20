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

// ---- Speech ----
// Chiku talks in two ways:
//  1. Voice clips: 16 kHz, 16-bit PCM WAV files in LittleFS /voice/<key>.wav
//     (upload on the web page, or put them in data/voice/ and run uploadfs).
//  2. "Cute talk": if there is no clip, a synthesised babble ("chi-ku-chi!")
//     with the pitch shape of the feeling (happy rises, sad falls, ...).
enum Babble : uint8_t { BAB_HAPPY, BAB_SAD, BAB_ANGRY, BAB_SURPRISE, BAB_SHY, BAB_LOVE, BAB_QUESTION, BAB_COUNT };
void babble(Babble style, uint8_t syllables = 3);
bool playWav(const char* path);                // queued; false if file missing
void say(const char* key, Babble fallback = BAB_HAPPY);   // clip /voice/<key>.wav or babble
bool hasClip(const char* key);
}  // namespace audio
