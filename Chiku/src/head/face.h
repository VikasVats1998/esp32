// face.h - Chiku's animated eyes (LVGL objects on a black background).
// Moods are tables of eye parameters; every frame the real values glide
// towards the target with easing. Blink, look direction and weather effects
// are layered on top. To add a mood: add it to Mood, then add one row to
// MOODS[] in face.cpp.
#pragma once
#include <Arduino.h>
#include <lvgl.h>

namespace face {

enum Mood : uint8_t {
  M_NORMAL, M_HAPPY, M_SAD, M_SLEEPY, M_ANGRY, M_SURPRISED,
  M_DIZZY, M_LOVE, M_THINKING, M_LISTENING,
  M_JEALOUS, M_SULK, M_SHY, M_EXCITED, M_SCARED, M_CRY,   // "feelings"
  MOOD_COUNT
};

enum Fx : uint8_t {
  FX_NONE, FX_SUN, FX_RAIN, FX_SNOW, FX_CLOUD, FX_THUNDER, FX_FOG, FX_HOT, FX_COLD
};

lv_obj_t* create(lv_obj_t* parent);     // builds all objects inside parent
void update(uint32_t dtMs);             // call at ~30 fps from the UI task

// These may be called from any task; the change is applied in update().
void setMood(Mood m, uint32_t holdMs = 0);   // holdMs>0: go back to base mood after
void setBaseMood(Mood m);                    // mood to rest in
void lookAt(float x, float y);               // -1..1, disables auto look for 3 s
void setFx(Fx fx);                           // weather effect (persistent)
void setSleeping(bool on);                   // eyes closed, very dim
void setShake(bool on);                      // jitter (alarm ringing)
Mood currentMood();
const char* moodName(Mood m);

}  // namespace face
