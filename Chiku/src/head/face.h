// face.h - Chiku's animated face, drawn the "UI kit" way: true black background, glowing shapes,
// colour = mood. Every mood is one row in MOODS[] (face.cpp): eye shape, size, lids, mouth, extras.
// Every frame the eye numbers glide towards the target (tweening), so any two moods morph smoothly;
// when the eye *shape* changes (rect -> heart) the eye squashes shut and opens in the new shape.
// Layered on top: blink (5 frames, 2-6 s, 20 % double blink), look-around (8-15 s), breathing,
// wake-up animation, talking mouth while Chiku speaks, battery looks, weather particles.
// To add a mood: add it to Mood (before MOOD_COUNT), then one row to MOODS[] in face.cpp.
#pragma once
#include <Arduino.h>
#include <lvgl.h>

namespace face {

enum Mood : uint8_t {
  M_NORMAL, M_HAPPY, M_SAD, M_SLEEPY, M_ANGRY, M_SURPRISED,
  M_DIZZY, M_LOVE, M_THINKING, M_LISTENING,
  M_JEALOUS, M_SULK, M_SHY, M_EXCITED, M_SCARED, M_CRY,     // Chiku's own feelings
  M_CURIOUS, M_CONFUSED, M_BORED, M_WINK, M_ERROR, M_SPEAKING,   // added with the UI kit
  MOOD_COUNT
};

enum Fx : uint8_t {
  FX_NONE, FX_SUN, FX_RAIN, FX_SNOW, FX_CLOUD, FX_THUNDER, FX_FOG, FX_HOT, FX_COLD
};

// Battery looks from the UI kit (they replace the resting face, emotions still show on top).
enum Status : uint8_t { ST_NONE, ST_LOW_BATTERY, ST_CHARGING };

lv_obj_t* create(lv_obj_t* parent);     // builds the face inside parent
void update(uint32_t dtMs);             // call at ~30 fps from the UI task

// These may be called from any task; the change is applied in update().
void setMood(Mood m, uint32_t holdMs = 0);   // holdMs>0: go back to base mood after
void setBaseMood(Mood m);                    // mood to rest in
Mood baseMood();
void lookAt(float x, float y);               // -1..1, disables auto look for 3 s
void setFx(Fx fx);                           // weather effect (persistent)
void wink(int side = 1);                     // short wink (UI kit wink face, about 0.6 s)
void rage(uint32_t ms);                      // furious: angry face + shaking for ms
void setSleeping(bool on);                   // deep-sleep look: dim lines, z z
void setShake(bool on);                      // jitter (alarm ringing)
void setStatus(Status s, int percent);       // low battery / charging look
void playWakeUp();                           // closed -> slit -> half -> open -> happy (120 ms each)
void setCaption(bool on);                    // show the mood name at the bottom (mood demo)
void flash(bool on);                         // all eyes white (reaction game)
Mood currentMood();
const char* moodName(Mood m);
bool moodFromName(const char* name, Mood& out);   // also accepts "neutral" for normal
uint32_t moodColor(Mood m);                  // 0xRRGGBB, colour = mood

}  // namespace face
