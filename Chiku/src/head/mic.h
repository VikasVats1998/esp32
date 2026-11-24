// mic.h - microphone: sound level meter + clap detection (single / double clap).
// Reads the ES7210 mic ADC through audio::micRead() in its own task. Ignores the
// robot's own speaker (no clap events while sound plays and for 400 ms after).
#pragma once
#include <Arduino.h>

namespace mic {
void init();                    // after audio::init(); starts the task
bool online();
int level();                    // 0..100 smoothed loudness (about -60..-10 dBFS)
int peak();                     // 0..100 loudest sample in the last block
float noiseFloor();             // 0..100 quiet-room level the detector adapted to
void useExternalReader(bool on);    // true while the speech engine reads the mic (we get its samples)
// UI kit additions
int db();                       // rough sound level in dB (not calibrated, MIC_DB_OFFSET), -1 if offline
uint32_t bursts();              // voice-like sounds heard so far (loud 0.25-3 s): setup mic check
// EV_LOUD is published for a sudden loud sound (not while Chiku itself plays a sound)
}  // namespace mic
