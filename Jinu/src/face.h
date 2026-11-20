// face.h - Jinu's animated eyes, drawn on a 320x172 RAM canvas at 30 fps.
// Moods are tables of eye parameters; the real values glide towards the target
// (easing). Blink, look direction and mood effects (tears, blush, sparkles, Zzz)
// are layered on top. To add a mood: add it to Mood and one row to MOODS[].
#pragma once
#include <Arduino.h>

namespace face {
enum Mood : uint8_t {
  M_NORMAL, M_HAPPY, M_SAD, M_SLEEPY, M_ANGRY, M_SURPRISED, M_DIZZY, M_LOVE, M_THINKING, M_LISTENING,
  M_JEALOUS, M_SULK, M_SHY, M_EXCITED, M_SCARED, M_CRY, MOOD_COUNT
};

void init();
void update(uint32_t dtMs);                 // draws one frame (call at FACE_FPS)
// Thread-safe setters (applied in update()):
void setMood(Mood m, uint32_t holdMs = 0);  // holdMs>0: back to base mood afterwards; 0: stay
void setBaseMood(Mood m);
void lookAt(float x, float y);              // -1..1, pauses auto-look for 3 s
void setSleeping(bool on);
Mood currentMood();
const char* moodName(Mood m);
void setTestPattern(int p);                 // 0 off, 1 red, 2 green, 3 blue, 4 white, 5 black, 6 grey ramp
uint32_t ledColor();
void setLedOverride(int32_t rgb);           // test: fixed colour (-1 = follow the mood)                        // current eye colour 0xRRGGBB (for the RGB LED)
}  // namespace face
