// stubs.cpp - fake versions of every hardware / network module the UI uses (simulator only).
#include <Arduino.h>
#include "sim.h"
#include "config.h"
#include "events.h"
#include "storage.h"
#include "power.h"
#include "net.h"
#include "rtc_time.h"
#include "alarm_timer.h"
#include "weather.h"
#include "camlink.h"
#include "sonar.h"
#include "servo.h"
#include "imu.h"
#include "mic.h"
#include "audio.h"
#include "motion.h"
#include "behavior.h"
#include "desk.h"
#include "display.h"
#include "touch.h"

SimState sim;
EspClass ESP;
uint32_t millis() { return sim.ms; }
void delay(uint32_t ms) { sim.ms += ms; }
long random(long lo, long hi) { return hi > lo ? lo + rand() % (hi - lo) : lo; }
long random(long hi) { return random(0, hi); }

namespace events {
bool publish(Type, int32_t, int32_t) { return true; }
const char* name(Type) { return "?"; }
}
namespace storage {
static Settings S;
static bool init_ = false;
Settings& get() {
  if (!init_) {
    init_ = true;
    memset(&S, 0, sizeof(S));
    strcpy(S.ssid, "Home_5G");
    S.brightness = 153; S.volume = 45; S.setupDone = true;
    for (auto& a : S.alarms) { a.days = 0x7F; a.hour = 7; a.minute = 0; }
    S.alarms[0] = {true, 7, 0, 0x1F};
    S.alarms[1] = {false, 9, 30, 0x60};
    S.alarms[2] = {true, 22, 30, 0x7F};
  }
  return S;
}
void save() {}
void clearWifi() {}
bool fsReady() { return true; }
}
namespace power {
State state() { return ACTIVE; }
int batteryPercent() { return sim.battery; }
bool charging() { return sim.charging; }
void keepAwake(bool) {}
void sleepNow() {}
void powerOff() {}
uint32_t idleMs() { return 0; }
bool lowBattery() { return sim.battery < LOW_BATTERY_PCT; }
}
namespace net {
bool connected() { return sim.wifi; }
bool portalActive() { return !sim.wifi; }
const char* ip() { return "192.168.1.42"; }
int rssi() { return -52; }
void restartWifi() {}
}
namespace rtc_time {
bool valid() { return true; }
bool localTime(struct tm& t) {
  memset(&t, 0, sizeof(t));
  t.tm_hour = 8; t.tm_min = 22; t.tm_sec = 37; t.tm_wday = 2; t.tm_mday = 24; t.tm_mon = 10; t.tm_year = 126;
  return true;
}
}
namespace alarm_timer {
bool ringing() { return sim.ringing >= 0; }
int ringingId() { return sim.ringing; }
void stop() {}
void testRing() {}
void snooze(uint8_t) {}
int startTimer(uint32_t) { return 0; }
void cancelTimer(uint8_t) {}
uint32_t timerRemainingMs(uint8_t) { return sim.timerLeftMs; }
uint32_t timerTotalMs(uint8_t) { return sim.timerTotalMs; }
int activeTimer() { return sim.timerOn ? 0 : -1; }
void pauseTimer(uint8_t, bool) {}
bool timerPaused(uint8_t) { return false; }
uint8_t pomodoros() { return 1; }
bool nextAlarmText(char* out, size_t n) { snprintf(out, n, "Wed 07:00"); return true; }
}
namespace weather {
static WeatherData W = {true, 6, 2, 9, 2, true, 0};
const WeatherData& data() { W.updatedMs = sim.ms; return W; }
const char* describe(int) { return "Partly cloudy"; }
}
namespace camlink {
bool alive() { return true; }
bool streamReady() { return true; }
void streamUrl(char* out, size_t n) { snprintf(out, n, "http://192.168.1.43/stream"); }
const char* camState() { return "on"; }
void wake() {}
void sleepCam() {}
void led(bool) {}
void snap() {}
}
namespace sonar { int distanceCm() { return sim.sonarCm; } }
namespace servo {
float current(uint8_t) { return 90; }
void setTarget(uint8_t, float, float) {}
void hold(uint8_t) {}
void releaseAll() {}
}
namespace imu {
bool online() { return true; }
void getAccel(float& x, float& y, float& z) { x = 0.09f; y = -0.99f; z = 0.05f; }
float tiltDeg() { return 3; }
}
namespace mic {
bool online() { return true; }
float level() { return sim.micLevel; }
int db() { return 48; }
uint32_t bursts() { return 2; }
}
namespace audio {
void play(Melody, bool) {}
void beep(uint16_t, uint16_t) {}
void babble(Babble, uint8_t) {}
float speechLevel() { return sim.speech; }
}
namespace motion {
const char* gaitName(Gait g) {
  static const char* N[] = {"home", "walk", "back", "left", "right", "bend", "shakeleg", "moonwalk", "dance", "jump", "wiggle",
    "happy", "excited", "angry", "scared", "sulk", "jealous", "shy", "love", "sad", "surprised", "tiltl", "tiltr",
    "sleepy", "curious", "confused", "bored", "dizzy", "wink", "thinking", "listen", "speak"};
  return g < G_COUNT ? N[g] : "?";
}
void stop() {}
}
namespace behavior {
static Mode m = MODE_AUTO;
Mode mode() { return m; }
void setMode(Mode x) { m = x; }
}
namespace desk {
State state() { State s{}; s.paired = true; s.ok = true; s.hv = true; s.h = 74.5f; strcpy(s.src, "display"); return s; }
void moveHold(int) {}
void command(const char*, int, const char*, float, int) {}
}
namespace display {
void setBrightness(uint8_t) {}
void setRotation(int deg) { sim.rotation = ((deg % 360) + 360) % 360; }
int rotation() { return sim.rotation; }
void toLogical(int16_t&, int16_t&) {}
}
namespace touch {
uint32_t holdMs() { return 0; }
void setGestures(GestureMode) {}
}
