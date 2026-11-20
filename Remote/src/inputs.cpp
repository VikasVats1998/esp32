// inputs.cpp - see inputs.h
#include "inputs.h"
#include "config.h"
#include "logbuf.h"
#include "mapping.h"
#include "remotelink.h"
#include "storage.h"

using namespace remotelink;

namespace inputs {

static State st = {};
static int centerRaw[2] = {2048, 2048};
static float smooth[2] = {0, 0};
static uint32_t stickReleasedAt = 0;

struct Btn { int pin; bool down; uint32_t downAt, upAt; uint8_t taps; bool longFired; };
static Btn mode = {PIN_BTN_MODE}, click = {PIN_JOY_SW};
static const int PINS[5] = {PIN_BTN_CAPTURE, PIN_BTN_ZOOM_IN, PIN_BTN_ZOOM_OUT, PIN_BTN_MODE, PIN_JOY_SW};
static bool raw[5];
static uint32_t pairHeld = 0, calibHeld = 0, calibEnd = 0;
static int calMin[2], calMax[2];
static bool calibActive = false;

// Action table: what a gesture does. (macro ids are defined in remotelink.h; the gimbal module decides which buttons they press)
struct Action { Macro macro; bool toggleFine; };
//                          tap1                          tap2                           tap3                    long
static const Action MODE_ACTIONS[4]  = {{MACRO_PHOTO_VIDEO, false}, {MACRO_SWITCH_CAMERA, false}, {MACRO_LOCK, false}, {MACRO_ACTIVE_TRACK, false}};
static const Action CLICK_ACTIONS[4] = {{MACRO_RECENTER, false},    {MACRO_FLIP, false},          {MACRO_SPORT, false}, {MACRO_NONE, true}};

const State& state() { return st; }
bool calibrating() { return calibActive; }
bool anyButtonDown() { for (bool r : raw) if (r) return true; return false; }

void init() {
  for (int p : PINS) pinMode(p, INPUT_PULLUP);
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  long sx = 0, sy = 0;
  for (int i = 0; i < 64; i++) { sx += analogRead(PIN_JOY_X); sy += analogRead(PIN_JOY_Y); delay(3); }
  centerRaw[0] = sx / 64;
  centerRaw[1] = sy / 64;
  st.fine = storage::get().fine;
  st.lastInputMs = millis();
  LOGI("inputs", "stick centre %d / %d (keep it at rest while switching on)", centerRaw[0], centerRaw[1]);
}

void startCalibration() {
  calibActive = true;
  calibEnd = millis() + CALIB_TIME_MS;
  for (int a = 0; a < 2; a++) { calMin[a] = 4095; calMax[a] = 0; }
  LOGI("inputs", "calibration: move the stick to every extreme for %u s", CALIB_TIME_MS / 1000);
}

// Reports a gesture of a button with a tap counter: 1/2/3 taps, 4 = long press. Returns 0 while nothing is decided.
static int gesture(Btn& b, bool isDown, uint32_t now) {
  int result = 0;
  if (isDown && !b.down) { b.down = true; b.downAt = now; b.longFired = false; }
  else if (!isDown && b.down) {
    b.down = false;
    if (!b.longFired) { b.taps++; b.upAt = now; }
    b.longFired = false;
  }
  if (b.down && !b.longFired && now - b.downAt > LONG_PRESS_MS) { b.longFired = true; b.taps = 0; result = 4; }
  if (!b.down && b.taps && (b.taps >= 3 || now - b.upAt > TAP_WINDOW_MS)) { result = b.taps > 3 ? 3 : b.taps; b.taps = 0; }
  return result;
}

static float axis(int pin, int a) {
  int v = (analogRead(pin) + analogRead(pin) + analogRead(pin) + analogRead(pin)) / 4;
  const StickCal& c = storage::get().cal[a];
  if (calibActive) { calMin[a] = min(calMin[a], v); calMax[a] = max(calMax[a], v); }
  return mapping::normalize(v, c.min, centerRaw[a], c.max);
}

Event update() {
  uint32_t now = millis();
  Event ev = EV_NONE;
  for (int i = 0; i < 5; i++) raw[i] = digitalRead(PINS[i]) == LOW;
  bool capture = raw[0], zin = raw[1], zout = raw[2], modeB = raw[3], clickB = raw[4];

  // ---- combos (pairing / calibration) ----
  bool pairCombo = capture && modeB, calibCombo = zin && zout;
  if (pairCombo) { if (!pairHeld) pairHeld = now; else if (now - pairHeld > PAIR_HOLD_MS) { pairHeld = 0xFFFFFFFF; ev = EV_PAIR; } }
  else pairHeld = 0;
  if (calibCombo) { if (!calibHeld) calibHeld = now; else if (now - calibHeld > CALIB_HOLD_MS) { calibHeld = 0xFFFFFFFF; startCalibration(); ev = EV_CALIBRATE; } }
  else calibHeld = 0;
  bool combo = pairCombo || calibCombo;

  // ---- held keys ----
  uint16_t keys = 0;
  if (!combo && !calibActive) {
    if (capture) keys |= KEY_SHUTTER;
    if (zin) keys |= KEY_ZOOM_IN;
    if (zout) keys |= KEY_ZOOM_OUT;
  }
  st.keys = keys;

  // ---- gestures ----
  int g1 = gesture(mode, modeB && !combo, now), g2 = gesture(click, clickB && !combo, now);
  auto run = [&](int g, const Action* table) {
    if (g < 1 || g > 4) return;
    const Action& a = table[g - 1];
    if (a.toggleFine) {
      st.fine = !st.fine;
      storage::get().fine = st.fine;
      storage::save();
      ev = EV_FINE_CHANGED;
      LOGI("inputs", "fine mode %s", st.fine ? "ON" : "off");
    } else if (a.macro != MACRO_NONE) {
      st.macro = a.macro;
      st.macroCount++;
      LOGI("inputs", "macro %u", a.macro);
    }
  };
  run(g1, MODE_ACTIONS);
  run(g2, CLICK_ACTIONS);

  // ---- stick ----
  float x = axis(PIN_JOY_X, 0), y = axis(PIN_JOY_Y, 1);
  if (JOY_SWAP_XY) { float t = x; x = y; y = t; }
  if (JOY_INVERT_X) x = -x;
  if (JOY_INVERT_Y) y = -y;
  float scale = st.fine ? JOY_FINE_SCALE : 1.f;
  float sxv = mapping::shape(x, JOY_DEADZONE, JOY_EXPO, scale), syv = mapping::shape(y, JOY_DEADZONE, JOY_EXPO, scale);
  smooth[0] = JOY_SMOOTHING * smooth[0] + (1.f - JOY_SMOOTHING) * sxv;
  smooth[1] = JOY_SMOOTHING * smooth[1] + (1.f - JOY_SMOOTHING) * syv;
  if (fabsf(smooth[0]) < 0.004f) smooth[0] = 0;
  if (fabsf(smooth[1]) < 0.004f) smooth[1] = 0;
  st.jx = mapping::toI8(smooth[0]);
  st.jy = mapping::toI8(smooth[1]);
  bool out = st.jx != 0 || st.jy != 0;
  if (out) stickReleasedAt = now;
  st.stickActive = !calibActive && (out || now - stickReleasedAt < 150);            // keep injecting 150 ms after release: smooth stop
  if (calibActive) { st.jx = st.jy = 0; }

  if (out || anyButtonDown()) st.lastInputMs = now;

  // ---- calibration end ----
  if (calibActive && (int32_t)(now - calibEnd) >= 0) {
    calibActive = false;
    Settings& s = storage::get();
    bool ok = true;
    for (int a = 0; a < 2; a++) {
      if (calMax[a] - calMin[a] < 1200) ok = false;
    }
    if (ok) {
      for (int a = 0; a < 2; a++) s.cal[a] = {(uint16_t)(calMin[a] + 40), (uint16_t)(calMax[a] - 40)};   // stay slightly inside the end stops
      storage::save();
      LOGI("inputs", "calibration saved: x %d..%d, y %d..%d", calMin[0], calMax[0], calMin[1], calMax[1]);
    } else LOGW("inputs", "calibration failed: the stick did not reach every end (x %d..%d, y %d..%d)", calMin[0], calMax[0], calMin[1], calMax[1]);
    ev = EV_CALIBRATE;
  }
  return ev;
}

}  // namespace inputs
