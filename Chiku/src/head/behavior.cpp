// behavior.cpp - see behavior.h. Edit this file to change how Chiku reacts.
#include "behavior.h"
#include "config.h"
#include "face.h"
#include "audio.h"
#include "motion.h"
#include "servo.h"
#include "alarm_timer.h"
#include "power.h"
#include "ui.h"
#include "weather.h"
#include "storage.h"
#include "camlink.h"
#include "pads.h"

namespace behavior {

static bool petting = false;
static uint32_t ringTick = 0;
static bool ringSide = false;

// Pick the weather effect for the face.
static face::Fx fxFor(const WeatherData& w) {
  int c = w.code;
  if (c >= 95) return face::FX_THUNDER;
  if ((c >= 51 && c <= 67) || (c >= 80 && c <= 82)) return face::FX_RAIN;
  if ((c >= 71 && c <= 77) || c == 85 || c == 86) return face::FX_SNOW;
  if (c == 45 || c == 48) return face::FX_FOG;
  if (w.temp >= 30) return face::FX_HOT;
  if (w.temp <= 0) return face::FX_COLD;
  if (c == 3) return face::FX_CLOUD;
  if (c <= 2 && w.isDay) return face::FX_SUN;
  return face::FX_NONE;
}

static void applyWeather() {
  const WeatherData& w = weather::data();
  if (!w.valid) return;
  face::Fx fx = fxFor(w);
  face::setFx(fx);
  switch (fx) {                                     // a short reaction to new weather
    case face::FX_SUN: face::setMood(face::M_HAPPY, 4000); break;
    case face::FX_RAIN: face::setMood(face::M_SAD, 3000); break;
    case face::FX_THUNDER: face::setMood(face::M_SURPRISED, 3000); break;
    case face::FX_HOT: face::setMood(face::M_SLEEPY, 3000); break;
    default: break;
  }
}

// ---------- feelings engine ----------
// affection: goes up when petted, slowly fades.   annoyance: goes up with leg pokes.
// lastTouch: used to feel lonely when ignored.
static float affection = 20, annoyance = 0;
static uint32_t lastTouch = 0, nextIdle = 0, lonelyAt = 0;
static uint32_t calmUntil = 0;       // ignore idle actions until then
static uint32_t stopMoveAt = 0;      // ends a long (looping) emote

static void emote(face::Mood m, uint32_t faceMs, motion::Gait g, uint8_t cycles, audio::Babble talk,
                  const char* clip = nullptr) {
  face::setMood(m, faceMs);
  events::publish(events::EV_MOTION_CMD, g, cycles);
  if (clip) audio::say(clip, talk); else audio::babble(talk, 3);
  calmUntil = millis() + 4000;
}

// What each pad does. ears: friendly. legs: poking (can annoy). body: cuddle/drama.
static void onPad(int pad, int gesture) {
  uint32_t now = millis();
  lastTouch = now;
  using namespace pads;
  int side = (pad == PAD_EAR_L || pad == PAD_LEG_L) ? -1 : 1;
  bool ear = pad == PAD_EAR_L || pad == PAD_EAR_R;
  bool leg = pad == PAD_LEG_L || pad == PAD_LEG_R;
  if (alarm_timer::ringing()) { alarm_timer::stop(); return; }

  if (ear || pad == PAD_BODY) { affection = fminf(100, affection + (gesture == LONG_START ? 15 : 8)); annoyance *= 0.7f; }
  if (leg && gesture != LONG_END) annoyance = fminf(120, annoyance + 22);

  if (gesture == LONG_END) {                       // hand lifted after a long press
    motion::stop();
    face::setMood(pad == PAD_LEG_L || pad == PAD_LEG_R ? face::M_SULK : face::M_LOVE, 1800);
    return;
  }
  // Pushed too far: legs poked again and again -> cross, then ruthna (sulk)
  if (leg && annoyance > 95) {
    annoyance = 40;
    emote(face::M_ANGRY, 2500, motion::G_ANGRY, 3, audio::BAB_ANGRY, "angry");
    nextIdle = now + 9000;
    stopMoveAt = 0;
    return;
  }
  if (leg && annoyance > 60 && gesture == TAP1) {
    emote(face::M_SULK, 4500, motion::G_SULK, 3, audio::BAB_SAD, "sulk");
    return;
  }
  face::lookAt(side * 0.8f, 0);
  switch (pad) {
    case PAD_EAR_L: case PAD_EAR_R:
      switch (gesture) {
        case TAP1: emote(face::M_HAPPY, 2200, side < 0 ? motion::G_TILT_L : motion::G_TILT_R, 1, audio::BAB_HAPPY, "happy"); break;
        case TAP2: emote(face::M_EXCITED, 3200, motion::G_EXCITED, 5, audio::BAB_HAPPY, "excited"); break;
        case TAP3: emote(face::M_SHY, 3500, motion::G_SHY, 3, audio::BAB_SHY, "shy"); break;
        case LONG_START: face::setMood(face::M_LOVE, 0); events::publish(events::EV_MOTION_CMD, motion::G_LOVE, 25);
                         audio::say("love", audio::BAB_LOVE); break;
      }
      break;
    case PAD_LEG_L: case PAD_LEG_R:
      switch (gesture) {
        case TAP1: emote(face::M_SURPRISED, 1600, motion::G_SURPRISED, 1, audio::BAB_SURPRISE, "surprised"); break;
        case TAP2: emote(face::M_ANGRY, 2500, motion::G_ANGRY, 3, audio::BAB_ANGRY, "angry"); annoyance += 15; break;
        case TAP3: emote(face::M_SCARED, 3200, motion::G_SCARED, 12, audio::BAB_SURPRISE, "scared"); break;
        case LONG_START: emote(face::M_SULK, 0, motion::G_SULK, 8, audio::BAB_SAD, "sulk"); break;
      }
      break;
    case PAD_BODY:
      switch (gesture) {
        case TAP1: emote(face::M_HAPPY, 2200, motion::G_HAPPY, 3, audio::BAB_HAPPY, "happy"); break;
        case TAP2: emote(face::M_JEALOUS, 3800, motion::G_JEALOUS, 5, audio::BAB_QUESTION, "jealous"); break;
        case TAP3: emote(face::M_CRY, 3500, motion::G_SAD, 2, audio::BAB_SAD, "sad"); break;
        case LONG_START: face::setMood(face::M_LOVE, 0); events::publish(events::EV_MOTION_CMD, motion::G_LOVE, 25);
                         audio::say("love", audio::BAB_LOVE); break;
      }
      break;
  }
}

// Small things Chiku does on its own so it feels alive.
static void idleLife(uint32_t now) {
  if (now < nextIdle || now < calmUntil) return;
  if (power::state() != power::ACTIVE || alarm_timer::ringing() || ui::current() != ui::S_FACE) return;
  nextIdle = now + random(25000, 50000);
  // Ignored for a long time: lonely
  if (lastTouch && now - lastTouch > 120000UL) {
    face::setMood(face::M_SAD, 3500);
    audio::babble(audio::BAB_SAD, 2);
    events::publish(events::EV_MOTION_CMD, motion::G_SAD, 1);
    return;
  }
  switch (random(0, 6)) {
    case 0: face::setMood(face::M_THINKING, 2500); events::publish(events::EV_MOTION_CMD, motion::G_TILT_L, 1); break;
    case 1: face::setMood(face::M_HAPPY, 1500); audio::babble(audio::BAB_HAPPY, 2); break;
    case 2: face::setMood(face::M_SLEEPY, 2500); break;                          // yawn
    case 3: face::setMood(affection > 60 ? face::M_LOVE : face::M_HAPPY, 2000);
            events::publish(events::EV_MOTION_CMD, motion::G_LOVE, 1); break;
    case 4: face::setMood(face::M_THINKING, 2500); events::publish(events::EV_MOTION_CMD, motion::G_TILT_R, 1);
            audio::babble(audio::BAB_QUESTION, 3); break;
    default: break;
  }
}

static Mode curMode = MODE_AUTO;
const char* modeName(Mode m) { return m == MODE_MANUAL ? "manual" : m == MODE_EXPLORE ? "explore" : "auto"; }
Mode mode() { return curMode; }
bool modeFromName(const char* s, Mode& out) {
  for (int i = 0; i < 3; i++) if (!strcmp(s, modeName((Mode)i))) { out = (Mode)i; return true; }
  return false;
}
void setMode(Mode m) {
  curMode = m;
  motion::setAvoidMode(m == MODE_EXPLORE);
  motion::setFollowLook(m != MODE_MANUAL);
  power::keepAwake(m == MODE_MANUAL);          // never dim or sleep while testing by hand
  if (m != MODE_MANUAL) nextIdle = millis() + 20000;
  face::setMood(m == MODE_EXPLORE ? face::M_EXCITED : face::M_NORMAL, m == MODE_EXPLORE ? 1500 : 0);
}

void init() { lastTouch = millis(); nextIdle = millis() + 30000; }

void onEvent(const events::Event& e) {
  using namespace events;
  bool ringing = alarm_timer::ringing();
  if (curMode == MODE_MANUAL) {                // testing by hand: only housekeeping events are handled
    switch (e.type) {
      case EV_ALARM_RING: case EV_ALARM_STOP: case EV_WEATHER: case EV_POWER: case EV_WAKE: case EV_SETTINGS: break;
      default: return;
    }
  }
  switch (e.type) {
    case EV_TOUCH_TAP:
      if (ringing) { alarm_timer::stop(); break; }
      if (ui::current() == ui::S_FACE) {
        if (random(0, 2)) { face::setMood(face::M_HAPPY, 1800); audio::play(audio::MEL_HAPPY); }
        else { face::setMood(face::M_SURPRISED, 1500); audio::beep(1200, 60); }
        float lx = (e.a - LCD_W / 2) / (LCD_W / 2.f), ly = (e.b - LCD_H / 2) / (LCD_H / 2.f);
        face::lookAt(lx, ly);                       // look at the finger
        events::publish(EV_LOOK, (int)(lx * 100), (int)(ly * 100));
      }
      break;
    case EV_SHAKE:
      if (ringing) { alarm_timer::stop(); break; }
      face::setMood(face::M_DIZZY, 3500);
      audio::play(audio::MEL_DIZZY);
      break;
    case EV_PICKUP: face::setMood(face::M_SURPRISED, 1500); audio::beep(1400, 50); break;
    case EV_PUTDOWN: face::setMood(face::M_HAPPY, 1200); break;
    case EV_TAP_IMU: face::setMood(face::M_SURPRISED, 800); break;
    case EV_FALL:
      // motion.cpp already stopped and released the servos
      face::setMood(face::M_SAD, 5000);
      audio::play(audio::MEL_SAD);
      break;
    case EV_PAD: onPad(e.a, e.b); break;
    case EV_OBSTACLE:
      face::setMood(face::M_SURPRISED, 1000);
      break;
    case EV_ALARM_RING:
      power::keepAwake(true);
      ui::show(ui::S_FACE);
      face::setMood(face::M_SURPRISED, 0);
      face::setShake(true);
      audio::say(e.a >= 100 ? "timer" : "alarm", audio::BAB_SURPRISE);   // spoken line first
      audio::play(e.a >= 100 ? audio::MEL_TIMER : audio::MEL_ALARM, true);
      break;
    case EV_ALARM_STOP:
      audio::stop();
      face::setShake(false);
      face::setMood(face::M_HAPPY, 2000);
      power::keepAwake(false);
      motion::stop();
      break;
    case EV_WEATHER: applyWeather(); break;
    case EV_POWER:
      face::setSleeping(e.a == power::SLEEP);
      break;
    case EV_WAKE:
      face::setMood(face::M_HAPPY, 1200);
      audio::say("hello", audio::BAB_HAPPY);
      break;
    case EV_SETTINGS:
      audio::setVolume(storage::get().volume);
      servo::applySettings();
      break;
    default: break;
  }
}

void update(uint32_t dtMs) {
  if (curMode == MODE_MANUAL) return;
  // feelings fade slowly
  affection = fmaxf(0, affection - dtMs * 0.0003f);
  annoyance = fmaxf(0, annoyance - dtMs * 0.006f);
  idleLife(millis());
  // While ringing: shake the head left and right
  if (alarm_timer::ringing()) {
    ringTick += dtMs;
    if (ringTick > 350) {
      ringTick = 0;
      ringSide = !ringSide;
      events::publish(events::EV_LOOK, ringSide ? 100 : -100, 0);
    }
  }
}

}  // namespace behavior
