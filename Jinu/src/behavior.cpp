// behavior.cpp - see behavior.h
#include "behavior.h"
#include "config.h"
#include "face.h"
#include "audio.h"
#include "motion.h"
#include "power.h"
#include "pads.h"
#include "storage.h"
#include "display.h"
#include <math.h>

namespace behavior {

// ---------- feelings ----------
// affection: rises when petted, fades.  annoyance: rises with leg pokes.
static float affection = 20, annoyance = 0;
static uint32_t lastTouch = 0, nextIdle = 0, calmUntil = 0, lastGreet = 0;
static uint32_t nearSince = 0;
static int prevDist = -1;

static void applyBrightness() {
  uint8_t b = storage::get().brightness;
  switch (power::state()) {
    case power::ACTIVE: display::setBrightness(b); break;
    case power::DIM: display::setBrightness(min<int>(b, BRIGHTNESS_DIM)); break;
    case power::SLEEP: display::setBrightness(BRIGHTNESS_SLEEP); break;
  }
}

// face + body language + voice in one call
static void emote(face::Mood m, uint32_t faceMs, motion::Gait g, uint8_t cycles, audio::Babble talk, const char* clip) {
  face::setMood(m, faceMs);
  events::publish(events::EV_MOTION_CMD, g, cycles);
  audio::say(clip, talk);
  calmUntil = millis() + 4000;
}

// What each pad does.  head pads: friendly.  legs: poking (can annoy).  body: cuddle / drama.
static void onPad(int pad, int gesture) {
  using namespace pads;
  uint32_t now = millis();
  lastTouch = now;
  int side = (pad == PAD_HEAD_L || pad == PAD_LEG_L) ? -1 : 1;
  bool head = pad == PAD_HEAD_L || pad == PAD_HEAD_R;
  bool leg = pad == PAD_LEG_L || pad == PAD_LEG_R;

  if (head || pad == PAD_BODY) { affection = fminf(100, affection + (gesture == LONG_START ? 15 : 8)); annoyance *= 0.7f; }
  if (leg && gesture != LONG_END) annoyance = fminf(120, annoyance + 22);

  if (gesture == LONG_END) {                                   // hand lifted after a long press
    events::publish(events::EV_MOTION_CMD, motion::G_HOME, 1);
    face::setMood(leg ? face::M_SULK : face::M_LOVE, 1800);
    return;
  }
  if (leg && annoyance > 95) {                                 // poked too often: cross, then ruthna
    annoyance = 40;
    emote(face::M_ANGRY, 2500, motion::G_ANGRY, 3, audio::BAB_ANGRY, "angry");
    nextIdle = now + 9000;
    return;
  }
  if (leg && annoyance > 60 && gesture == TAP1) {
    emote(face::M_SULK, 4500, motion::G_SULK, 3, audio::BAB_SAD, "sulk");
    return;
  }
  face::lookAt(side * 0.8f, 0);
  switch (pad) {
    case PAD_HEAD_L: case PAD_HEAD_R:
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

// A hand coming close (sonar): notice it, then greet.
static void onDistance(int cm) {
  uint32_t now = millis();
  if (cm < GREET_NEAR_CM && (prevDist < 0 || prevDist >= GREET_NEAR_CM + 15)) {
    nearSince = now;
    power::activity();
    if (now - lastGreet > 15000 && !motion::busy()) {
      lastGreet = now;
      face::setMood(face::M_SURPRISED, 700);
      audio::beep(1300, 50);
      calmUntil = now + 3000;
    }
  }
  prevDist = cm;
}

// Small things Jinu does on its own so it feels alive.
static void idleLife(uint32_t now) {
  if (now < nextIdle || now < calmUntil || power::state() != power::ACTIVE || motion::busy()) return;
  nextIdle = now + random(25000, 50000);
  if (lastTouch && now - lastTouch > 120000UL) {               // ignored for 2 minutes: lonely
    face::setMood(face::M_SAD, 3500);
    audio::babble(audio::BAB_SAD, 2);
    events::publish(events::EV_MOTION_CMD, motion::G_SAD, 1);
    return;
  }
  switch (random(0, 6)) {
    case 0: face::setMood(face::M_THINKING, 2500); events::publish(events::EV_MOTION_CMD, motion::G_TILT_L, 1); break;
    case 1: face::setMood(face::M_HAPPY, 1500); audio::babble(audio::BAB_HAPPY, 2); break;
    case 2: face::setMood(face::M_SLEEPY, 2500); break;
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
  power::keepAwake(m == MODE_MANUAL);          // never dim or sleep while testing by hand
  if (m != MODE_MANUAL) nextIdle = millis() + 20000;
  face::setMood(m == MODE_EXPLORE ? face::M_EXCITED : face::M_NORMAL, m == MODE_EXPLORE ? 1500 : 0);
}

void init() { lastTouch = millis(); nextIdle = millis() + 30000; applyBrightness(); }

void onEvent(const events::Event& e) {
  using namespace events;
  if (curMode == MODE_MANUAL) {                // testing by hand: only housekeeping events are handled
    switch (e.type) {
      case EV_POWER: case EV_WAKE: case EV_SETTINGS: break;
      default: return;
    }
  }
  switch (e.type) {
    case EV_PAD: power::activity(); onPad(e.a, e.b); break;
    case EV_BUTTON:
      power::activity();
      if (e.a == 1) {                                          // short press: explore mode on / off
        setMode(curMode == MODE_EXPLORE ? MODE_AUTO : MODE_EXPLORE);
        audio::beep(curMode == MODE_EXPLORE ? 1500 : 900, 80);
      }
      break;
    case EV_DIST: onDistance(e.a); break;
    case EV_OBSTACLE: face::setMood(face::M_SURPRISED, 1000); break;
    case EV_POWER: face::setSleeping(e.a == power::SLEEP); applyBrightness(); break;
    case EV_WAKE: face::setMood(face::M_HAPPY, 1200); audio::say("hello", audio::BAB_HAPPY); break;
    case EV_SETTINGS: audio::setVolume(storage::get().volume); applyBrightness(); break;
    default: break;
  }
}

void update(uint32_t dtMs) {
  if (curMode == MODE_MANUAL) return;
  affection = fmaxf(0, affection - dtMs * 0.0003f);
  annoyance = fmaxf(0, annoyance - dtMs * 0.006f);
  idleLife(millis());
}
}  // namespace behavior
