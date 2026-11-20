// buttons.cpp - see buttons.h
#include "buttons.h"
#include "config.h"
#include "height.h"
#include "logbuf.h"
#include "storage.h"

namespace buttons {

// The network task and the control task both use this module: one lock around every public function.
static SemaphoreHandle_t mtx = nullptr;
struct Lock {
  Lock() { if (!mtx) mtx = xSemaphoreCreateRecursiveMutex(); xSemaphoreTakeRecursive(mtx, portMAX_DELAY); }
  ~Lock() { xSemaphoreGiveRecursive(mtx); }
};

static const uint8_t PINS[K_COUNT] = {PIN_KEY_UP, PIN_KEY_DOWN, PIN_KEY_M1, PIN_KEY_M2, PIN_KEY_M3, PIN_KEY_M4, PIN_KEY_S, PIN_KEY_X};
static const char* NAMES[K_COUNT] = {"up", "down", "1", "2", "3", "4", "s", "x"};

struct KeyState { bool on; uint32_t offAt; };
static KeyState ks[K_COUNT];

// movement bookkeeping
static uint32_t runStart = 0;
static uint32_t lockUntil = 0;
static uint32_t lastUpdate = 0;
static uint32_t lastActivity = 0;
static struct { bool active; Key key; uint32_t at; uint32_t ms; } pending = {};     // delayed press after a direction change

// press sequences (save memory: S, pause, n)
struct Step { Key key; uint16_t pressMs; uint16_t gapMs; };
static struct { bool active; Step steps[2]; uint8_t count, index; uint32_t nextAt; bool pressing; } seq = {};

// preset travel
static bool travel = false;
static uint32_t travelEnd = 0;
static Key travelKey = K_M1;

const char* keyName(Key k) { return k < K_COUNT ? NAMES[k] : "?"; }
bool keyFromName(const char* n, Key& out) {
  for (int i = 0; i < K_COUNT; i++) if (!strcasecmp(n, NAMES[i])) { out = (Key)i; return true; }
  return false;
}

static void drive(Key k, bool on) {
  if (ks[k].on == on) return;
  ks[k].on = on;
  digitalWrite(PINS[k], on ? HIGH : LOW);
  lastActivity = millis();
  LOGD("keys", "%s %s", NAMES[k], on ? "pressed" : "released");
}

void init() {
  for (uint8_t i = 0; i < K_COUNT; i++) {
    pinMode(PINS[i], OUTPUT);
    digitalWrite(PINS[i], LOW);
  }
}

bool pressed(Key k) { return k < K_COUNT && ks[k].on; }
int moveDir() { return ks[K_UP].on ? 1 : ks[K_DOWN].on ? -1 : 0; }
bool travelling() { return travel; }
uint32_t lastActivityMs() { return lastActivity; }

static const char* checkAllowed() {
  if (!storage::get().armed) return "disarmed (child lock is on)";
  if (millis() < lockUntil) return "cooling down after the run limit";
  return nullptr;
}

static void releaseAll() {
  for (uint8_t i = 0; i < K_COUNT; i++) drive((Key)i, false);
  pending.active = false;
  seq.active = false;
}

const char* hold(Key k, uint32_t ms) {
  Lock lock;
  if (k != K_UP && k != K_DOWN) return "only up/down can be held";
  if (const char* e = checkAllowed()) return e;
  if (seq.active) return "busy with a key sequence";
  for (uint8_t i = K_M1; i < K_COUNT; i++) if (ks[i].on) return "another key is pressed";
  ms = constrain(ms, (uint32_t)TAP_MIN_MS, (uint32_t)HOLD_KEEPALIVE_MS * 2);
  uint32_t now = millis();
  Key other = k == K_UP ? K_DOWN : K_UP;
  if (ks[other].on) {                                    // direction change: release first, press after a pause
    drive(other, false);
    pending = {true, k, now + REVERSE_GAP_MS, ms};
    return nullptr;
  }
  if (pending.active && pending.key == k) { pending.ms = ms; return nullptr; }
  if (!ks[k].on) {
    if (now - lastActivity < 40 && !pending.active) { /* debounce for the desk controller */ }
    runStart = now;
    travel = false;                                      // a manual move cancels a preset travel
    LOGI("keys", "move %s", k == K_UP ? "up" : "down");
  }
  drive(k, true);
  ks[k].offAt = now + ms;                                // keep-alive: every call pushes the release time
  return nullptr;
}

const char* tap(Key k, uint32_t ms) {
  Lock lock;
  if (k >= K_COUNT) return "unknown key";
  ms = constrain(ms, (uint32_t)TAP_MIN_MS, (uint32_t)TAP_MAX_MS);
  if (k == K_UP || k == K_DOWN) return hold(k, min<uint32_t>(ms, HOLD_KEEPALIVE_MS * 2));
  if (const char* e = checkAllowed()) return e;
  if (seq.active) return "busy with a key sequence";
  if (ks[K_UP].on || ks[K_DOWN].on) return "desk is moving: stop first";
  drive(k, true);
  ks[k].offAt = millis() + ms;
  LOGI("keys", "tap %s %u ms", NAMES[k], (unsigned)ms);
  return nullptr;
}

const char* preset(uint8_t n) {
  Lock lock;
  if (n < 1 || n > 4) return "preset must be 1..4";
  Key k = (Key)(K_M1 + n - 1);
  if (const char* e = checkAllowed()) return e;
  if (ks[K_UP].on || ks[K_DOWN].on) return "desk is moving: stop first";
  travel = true;
  travelKey = k;
  height::invalidateEstimate();
  bool hold = storage::get().presetHold;
  travelEnd = millis() + PRESET_HOLD_MAX_MS;
  LOGI("keys", "go to preset %u (%s)", n, hold ? "key held until the desk stops" : "tap");
  drive(k, true);
  ks[k].offAt = millis() + (hold ? PRESET_HOLD_MAX_MS : PRESET_TAP_MS);
  return nullptr;
}

const char* saveMemory(uint8_t n) {
  Lock lock;
  if (n < 1 || n > 4) return "memory must be 1..4";
  if (const char* e = checkAllowed()) return e;
  if (seq.active || ks[K_UP].on || ks[K_DOWN].on) return "busy";
  seq.active = true;
  seq.count = 2;
  seq.index = 0;
  seq.pressing = false;
  seq.steps[0] = {K_S, 150, 350};
  seq.steps[1] = {(Key)(K_M1 + n - 1), 150, 0};
  seq.nextAt = millis();
  LOGI("keys", "save current height in memory %u (S, then %u)", n, n);
  return nullptr;
}

void stop() {
  Lock lock;
  bool wasMoving = moveDir() != 0;
  bool hadTravel = travel;
  releaseAll();
  if (hadTravel && !wasMoving) {                         // a preset travel runs by itself: any key press stops it
    drive(K_UP, true);
    ks[K_UP].offAt = millis() + STOP_KEY_TAP_MS;
    LOGW("keys", "stop: tapped UP to cancel the preset travel");
  }
  travel = false;
  LOGI("keys", "stop");
}

void travelDone() {
  Lock lock;
  if (!travel) return;
  travel = false;
  if (storage::get().presetHold) drive(travelKey, false);
  LOGI("keys", "preset travel finished");
}

void update() {
  Lock lock;
  uint32_t now = millis();
  uint32_t dt = now - lastUpdate;
  lastUpdate = now;
  for (uint8_t i = 0; i < K_COUNT; i++) {
    if (ks[i].on && (int32_t)(now - ks[i].offAt) >= 0) drive((Key)i, false);
  }
  if (pending.active && (int32_t)(now - pending.at) >= 0) {
    pending.active = false;
    runStart = now;
    drive(pending.key, true);
    ks[pending.key].offAt = now + pending.ms;
    LOGI("keys", "move %s (after direction change)", pending.key == K_UP ? "up" : "down");
  }
  // run-time limit
  if (moveDir() != 0 && now - runStart > MAX_RUN_MS) {
    LOGW("keys", "run limit %u s reached: released", MAX_RUN_MS / 1000);
    releaseAll();
    lockUntil = now + LOCKOUT_AFTER_MAX_MS;
  }
  // travel timeout
  if (travel && (int32_t)(now - travelEnd) >= 0) {
    LOGW("keys", "preset travel timeout");
    travelDone();
    drive(travelKey, false);
  }
  // key sequence player
  if (seq.active && (int32_t)(now - seq.nextAt) >= 0) {
    Step& st = seq.steps[seq.index];
    if (!seq.pressing) {
      drive(st.key, true);
      ks[st.key].offAt = now + st.pressMs;
      seq.pressing = true;
      seq.nextAt = now + st.pressMs + st.gapMs;
    } else {
      seq.pressing = false;
      if (++seq.index >= seq.count) seq.active = false;
      else seq.nextAt = now;
    }
  }
  height::noteMotion(moveDir(), dt);
}

}  // namespace buttons
