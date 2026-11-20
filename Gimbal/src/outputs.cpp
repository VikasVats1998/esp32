// outputs.cpp - see outputs.h
#include "outputs.h"
#include "config.h"
#include "logbuf.h"
#include "remotelink.h"
#include "storage.h"

namespace outputs {

static const int8_t KEY_PINS[6] = {PIN_KEY_SHUTTER, PIN_KEY_M, PIN_KEY_TRIGGER, PIN_KEY_ZOOM_IN, PIN_KEY_ZOOM_OUT, PIN_KEY_X};
static const char* KEY_NAMES[6] = {"shutter", "m", "trigger", "zoomin", "zoomout", "x"};
static const int8_t OUT_PINS[3] = {PIN_JOY_X_OUT, PIN_JOY_Y_OUT, ZOOM_ANALOG ? PIN_ZOOM_OUT : -1};
static const int8_t IN_PINS[3] = {PIN_JOY_X_IN, PIN_JOY_Y_IN, PIN_ZOOM_IN};

static bool pwmOn[3] = {};
static uint16_t remoteKeys = 0, macroKeys = 0, pressed = 0;
static uint32_t pressedSince[6] = {};

// learn state
static bool learnActive = false;
static uint32_t learnStartMs = 0;
static int mvMin[3], mvMax[3];
static long mvSum[3];
static int mvN = 0;

const char* keyName(int b) { return b >= 0 && b < 6 ? KEY_NAMES[b] : "?"; }
bool keyFromName(const char* n, int& bit) {
  for (int i = 0; i < 6; i++) if (!strcasecmp(n, KEY_NAMES[i])) { bit = i; return true; }
  return false;
}

static void applyKeys() {
  uint16_t want = remoteKeys | macroKeys;
  uint32_t now = millis();
  for (int i = 0; i < 6; i++) {
    bool on = (want >> i) & 1;
    bool was = (pressed >> i) & 1;
    if (on == was || KEY_PINS[i] < 0) continue;
    digitalWrite(KEY_PINS[i], (on == (bool)KEY_PRESS_ACTIVE_HIGH) ? HIGH : LOW);
    if (on) pressedSince[i] = now;
    pressed = on ? (pressed | (1 << i)) : (pressed & ~(1 << i));
    LOGD("keys", "%s %s", KEY_NAMES[i], on ? "pressed" : "released");
  }
}

void init() {
  for (int i = 0; i < 6; i++) {
    if (KEY_PINS[i] < 0) continue;
    pinMode(KEY_PINS[i], OUTPUT);
    digitalWrite(KEY_PINS[i], KEY_PRESS_ACTIVE_HIGH ? LOW : HIGH);
  }
  for (int a = 0; a < 3; a++) if (OUT_PINS[a] >= 0) pinMode(OUT_PINS[a], INPUT);       // Hi-Z
}

static uint32_t toCounts(const Axis& ax, int8_t v) {
  int c = ax.center;
  int out = v >= 0 ? c + (int)v * (ax.max - c) / 127 : c + (int)v * (c - ax.min) / 127;
  return constrain(out, 0, (1 << PWM_BITS) - 1);
}

static void drive(int axis, int8_t v) {
  if (OUT_PINS[axis] < 0) return;
  if (!pwmOn[axis]) { ledcAttach(OUT_PINS[axis], PWM_FREQ_HZ, PWM_BITS); pwmOn[axis] = true; }
  ledcWrite(OUT_PINS[axis], toCounts(storage::get().axis[axis], v));
}

void setStick(int8_t x, int8_t y, int8_t zoom) {
  if (learnActive) return;
  drive(0, x);
  drive(1, y);
  if (ZOOM_ANALOG) drive(2, zoom);
}

void centerSticks() { setStick(0, 0, 0); }

void releaseSticks() {
  for (int a = 0; a < 3; a++) {
    if (!pwmOn[a]) continue;
    ledcDetach(OUT_PINS[a]);
    pinMode(OUT_PINS[a], INPUT);
    pwmOn[a] = false;
  }
}

bool injecting() { return pwmOn[0] || pwmOn[1] || pwmOn[2]; }

void setRemoteKeys(uint16_t m) { remoteKeys = m & 0x3F; applyKeys(); }
void setMacroKeys(uint16_t m) { macroKeys = m & 0x3F; applyKeys(); }
void releaseAllKeys() { remoteKeys = macroKeys = 0; applyKeys(); }
uint16_t keysPressed() { return pressed; }

void update() {
  uint32_t now = millis();
  for (int i = 0; i < 6; i++) {
    if (((pressed >> i) & 1) && now - pressedSince[i] > MAX_KEY_HOLD_MS) {
      LOGW("keys", "%s held longer than %u s: released", KEY_NAMES[i], MAX_KEY_HOLD_MS / 1000);
      remoteKeys &= ~(1 << i);
      macroKeys &= ~(1 << i);
      applyKeys();
    }
  }
  if (learnActive) {
    for (int a = 0; a < 3; a++) {
      if (IN_PINS[a] < 0) continue;
      int mv = analogReadMilliVolts(IN_PINS[a]);
      mvMin[a] = min(mvMin[a], mv);
      mvMax[a] = max(mvMax[a], mv);
      if (millis() - learnStartMs < 400) mvSum[a] += mv;             // first 0.4 s: stick at rest = centre
    }
    if (millis() - learnStartMs < 400) mvN++;
  }
}

// ---- learning the physical joystick ----
bool canLearn() { return PIN_JOY_X_IN >= 0 && PIN_JOY_Y_IN >= 0; }
bool learning() { return learnActive; }
int wiperMv(int axis) { return axis >= 0 && axis < 3 && IN_PINS[axis] >= 0 ? analogReadMilliVolts(IN_PINS[axis]) : -1; }

void learnStart() {
  releaseSticks();
  for (int a = 0; a < 3; a++) { mvMin[a] = 5000; mvMax[a] = 0; mvSum[a] = 0; }
  mvN = 0;
  learnStartMs = millis();
  learnActive = true;
  LOGI("learn", "started: keep the physical joystick at rest for 1 s, then move it to every extreme, then 'learn stop'");
}

bool learnStop() {
  if (!learnActive) return false;
  learnActive = false;
  Settings& s = storage::get();
  bool ok = false;
  for (int a = 0; a < (ZOOM_ANALOG ? 3 : 2); a++) {
    if (IN_PINS[a] < 0 || mvN == 0) continue;
    int centerMv = mvSum[a] / mvN;
    auto counts = [](int mv) { return (uint16_t)constrain(mv * 1023L / 3300, 0, 1023); };
    if (mvMax[a] - mvMin[a] < 300) { LOGW("learn", "axis %d barely moved (%d mV): not saved", a, mvMax[a] - mvMin[a]); continue; }
    s.axis[a] = {counts(mvMin[a] + 20), counts(centerMv), counts(mvMax[a] - 20)};   // stay a little inside the end stops
    LOGI("learn", "axis %d: min %u center %u max %u counts (%d / %d / %d mV)", a, s.axis[a].min, s.axis[a].center, s.axis[a].max, mvMin[a], centerMv, mvMax[a]);
    ok = true;
  }
  if (ok) storage::save();
  return ok;
}

}  // namespace outputs
