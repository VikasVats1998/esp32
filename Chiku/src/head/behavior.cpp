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

void init() {}

void onEvent(const events::Event& e) {
  using namespace events;
  bool ringing = alarm_timer::ringing();
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
    case EV_PET:
      if (e.a) {
        petting = true;
        face::setMood(face::M_LOVE, 0);
        audio::play(audio::MEL_PET);
        events::publish(EV_MOTION_CMD, motion::G_WIGGLE, 3);
      } else {
        petting = false;
        face::setMood(face::M_LOVE, 1500);
      }
      break;
    case EV_OBSTACLE:
      face::setMood(face::M_SURPRISED, 1000);
      break;
    case EV_ALARM_RING:
      power::keepAwake(true);
      ui::show(ui::S_FACE);
      face::setMood(face::M_SURPRISED, 0);
      face::setShake(true);
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
      break;
    case EV_SETTINGS:
      audio::setVolume(storage::get().volume);
      servo::applySettings();
      break;
    default: break;
  }
}

void update(uint32_t dtMs) {
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
