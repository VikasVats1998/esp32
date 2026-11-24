// face.cpp - see face.h. Drawing is in face_draw.cpp; this file decides WHAT to draw each frame.
#include "face.h"
#include "face_draw.h"
#include "config.h"
#include "events.h"
#include "audio.h"
#include "mic.h"
#include <math.h>

namespace face {

// ---------- mood table (UI kit, page 2) ----------
// Eye: kind, width, height, corner radius %, lid inner %, lid outer %, x / y offset (px).
// Round kinds (arc, heart, star, spiral, cross) use width as their size.
struct EyeDef { uint8_t kind; int16_t w, h; uint8_t round, lidIn, lidOut; int8_t dx, dy; };
struct MoodDef {
  const char* name;
  uint32_t color;            // colour = mood
  EyeDef eye[2];             // left, right
  uint8_t mouth;             // MouthKind
  uint16_t extras;           // Extra bits
  int8_t lookX, lookY;       // forced look -100..100, FREE = eyes wander
  bool blink;
};
static const int8_t FREE = 127;
#define RECT(w, h, r, li, lo, dx, dy) {K_RECT, w, h, r, li, lo, dx, dy}
#define SHAPE(k, w, dy) {k, w, w, 0, 0, 0, 0, dy}
#define BOTH(e) {e, e}

static const uint32_t CYAN = 0x00F0FF, PINK = 0xFF6BCB, YELLOW = 0xFFE135, LBLUE = 0x7CC8FF, RED = 0xFF4D4D,
                      GREEN = 0x4ADE80, GREY = 0xA9B4C2;

static const MoodDef MOODS[MOOD_COUNT] = {
  {"normal",    CYAN,     BOTH(RECT(106, 134, 26, 0, 0, 0, 0)),   MO_NONE,  0,           FREE, FREE, true},
  {"happy",     CYAN,     BOTH(SHAPE(K_ARC_UP, 112, 6)),           MO_SMILE, 0,           FREE, FREE, false},
  {"sad",       LBLUE,    BOTH(RECT(106, 112, 24, 0, 40, 0, 12)),  MO_NONE,  EX_TEAR,     0,    40,   true},
  {"sleepy",    0xF29BFF, BOTH(RECT(110, 34, 50, 0, 0, 0, 40)),    MO_NONE,  EX_ZZZ,      0,    0,    false},
  {"angry",     RED,      BOTH(RECT(106, 112, 22, 42, 0, 0, 8)),   MO_FROWN, 0,           FREE, FREE, true},
  {"surprised", CYAN,     BOTH(RECT(136, 136, 50, 0, 0, 0, -6)),   MO_O,     0,           0,    -15,  true},
  {"dizzy",     GREEN,    BOTH(SHAPE(K_SPIRAL, 126, 0)),           MO_WAVE,  0,           0,    0,    false},
  {"love",      PINK,     BOTH(SHAPE(K_HEART, 108, 0)),            MO_NONE,  EX_BLUSH,    0,    0,    false},
  {"thinking",  CYAN,     BOTH(RECT(106, 134, 26, 0, 0, 0, -12)),  MO_DOTS,  0,           30,   -60,  true},
  {"listening", CYAN,     BOTH(RECT(112, 140, 26, 0, 0, 0, -18)),  MO_BARS,  EX_RING,     0,    0,    true},
  {"jealous",   GREEN,    BOTH(RECT(106, 82, 22, 24, 24, 0, 10)),  MO_FLAT,  0,           90,   5,    true},
  {"sulk",      0x8090B0, BOTH(RECT(98, 96, 24, 10, 36, 0, 18)),   MO_FROWN, 0,           -90,  50,   true},
  {"shy",       0xFF9AC8, BOTH(RECT(86, 104, 30, 0, 0, 0, 14)),    MO_SMILE, EX_BLUSH,    -60,  70,   true},
  {"excited",   YELLOW,   BOTH(SHAPE(K_STAR, 116, 0)),             MO_D,     0,           0,    0,    false},
  {"scared",    LBLUE,    BOTH(RECT(66, 92, 40, 0, 0, 0, 10)),     MO_NONE,  EX_SWEAT,    0,    -10,  true},
  {"cry",       0x70A8FF, BOTH(RECT(106, 112, 24, 0, 40, 0, 12)),  MO_FROWN, EX_TEARS,    0,    40,   true},
  {"curious",   CYAN,     {RECT(70, 72, 30, 0, 0, 8, 24), RECT(112, 136, 26, 0, 0, 0, -4)}, MO_NONE, 0, FREE, FREE, true},
  {"confused",  0xFFD43B, {RECT(96, 120, 24, 0, 0, 0, 0), RECT(98, 96, 22, 36, 0, 0, 14)},   MO_NONE, EX_QUESTION, 0, 0, true},
  {"bored",     GREY,     BOTH(RECT(108, 70, 24, 0, 0, 0, 10)),    MO_FLAT,  0,           FREE, FREE, true},
  {"wink",      CYAN,     {RECT(106, 134, 26, 0, 0, 0, 0), SHAPE(K_ARC_UP, 110, 6)},  MO_SMILE, 0, FREE, FREE, false},
  {"error",     RED,      BOTH(SHAPE(K_CROSS, 120, 0)),            MO_FLAT,  0,           0,    0,    false},
  {"speaking",  CYAN,     BOTH(SHAPE(K_ARC_UP, 112, 6)),           MO_TALK,  0,           FREE, FREE, false},
};

const char* moodName(Mood m) { return m < MOOD_COUNT ? MOODS[m].name : "?"; }
uint32_t moodColor(Mood m) { return m < MOOD_COUNT ? MOODS[m].color : CYAN; }
bool moodFromName(const char* n, Mood& out) {
  if (!strcmp(n, "neutral")) { out = M_NORMAL; return true; }
  for (int i = 0; i < MOOD_COUNT; i++) if (!strcmp(n, MOODS[i].name)) { out = (Mood)i; return true; }
  return false;
}

// Special looks that are not moods (UI kit pages 4 and 3)
static const MoodDef LOOK_LOWBATT = {"", 0xFFD43B, BOTH(RECT(112, 66, 30, 0, 0, 0, -16)), MO_NONE, EX_BATTERY, FREE, FREE, true};
static const MoodDef LOOK_CHARGE = {"", GREEN, BOTH(SHAPE(K_ARC_UP, 112, -14)), MO_NONE, EX_CHARGE_RING, 0, 0, false};
static const MoodDef LOOK_SLEEP = {"", 0x3A3A86, BOTH(RECT(84, 6, 3, 0, 0, 0, 30)), MO_NONE, EX_ZZZ, 0, 0, false};
static const EyeDef WAKE_STEPS[5] = {
  SHAPE(K_ARC_DOWN, 100, 10), RECT(112, 10, 5, 0, 0, 0, 0), RECT(112, 66, 26, 0, 0, 0, 0),
  RECT(106, 134, 26, 0, 0, 0, 0), SHAPE(K_ARC_UP, 112, 6)};

// ---------- layout ----------
static const float CX = LCD_W / 2, EYE_Y = 208, EYE_GAP = 168, MOUTH_Y = 330;
static const float LOOK_PX_X = 55, LOOK_PX_Y = 35;   // look-around range (UI kit)

// ---------- requests from other tasks ----------
static volatile Mood reqMood = M_NORMAL;
static volatile bool reqMoodFlag = false;
static volatile uint32_t reqHold = 0;
static volatile Mood base = M_NORMAL;
static volatile Fx fxMode = FX_NONE;
static volatile bool sleeping = false, shaking = false, captionOn = false, flashOn = false, wakeReq = false;
static volatile Status status = ST_NONE;
static volatile int statusPct = 0;
static volatile float reqLookX = 0, reqLookY = 0;
static volatile bool reqLookFlag = false;

// ---------- state ----------
struct EyeState { float x, y, w, h, round, lidIn, lidOut; uint8_t kind; uint8_t nextKind; float squash; };
static EyeState es[2];
static float cr = 0, cg = 240, cb = 255;
static Mood mood = M_NORMAL;
static uint32_t holdLeft = 0;
static float lookX = 0, lookY = 0, lookTX = 0, lookTY = 0;   // px
static uint32_t autoLookBlockUntil = 0, nextLook = 4000, lookBackAt = 0, neckEvtAt = 0;
static uint32_t blinkIn = 3000, blinkT = 0;
static bool doubleBlink = false;
static int wakeStep = 0;          // 0 = not waking, 1..5 frames, 6 = hold happy
static uint32_t wakeT = 0;
static float t = 0, spin = 0, talk = 0;
static uint32_t nowMs = 0;
static lv_obj_t* root = nullptr;
static Frame frame, lastFrame;
static bool haveLast = false;
static char textBuf[48], captionBuf[24];

// Weather particles (simple LVGL objects that fly over the face)
struct Particle { lv_obj_t* o; float x, y, vx, vy, life; };
static const int NP = 14;
static Particle parts[NP];
static lv_obj_t* flashObj = nullptr;
static Fx shownFx = FX_NONE;
static uint32_t nextFlash = 4000;
static float flashOpa = 0;

static lv_obj_t* plain(lv_obj_t* parent) {
  lv_obj_t* o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_remove_flag(o, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
  return o;
}

static void drawCb(lv_event_t* e) { drawFrame(lv_event_get_layer(e), frame); }

lv_obj_t* create(lv_obj_t* parent) {
  root = plain(parent);
  lv_obj_set_size(root, LCD_W, LCD_H);
  lv_obj_set_pos(root, 0, 0);
  lv_obj_set_style_bg_color(root, lv_color_black(), 0);
  lv_obj_add_event_cb(root, drawCb, LV_EVENT_DRAW_MAIN_END, nullptr);
  for (int i = 0; i < NP; i++) {
    parts[i].o = plain(root);
    lv_obj_add_flag(parts[i].o, LV_OBJ_FLAG_HIDDEN);
  }
  flashObj = plain(root);
  lv_obj_set_size(flashObj, LCD_W, LCD_H);
  lv_obj_set_style_bg_color(flashObj, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(flashObj, LV_OPA_TRANSP, 0);
  lv_obj_add_flag(flashObj, LV_OBJ_FLAG_HIDDEN);
  for (int i = 0; i < 2; i++) {
    const EyeDef& d = MOODS[M_NORMAL].eye[i];
    es[i] = {0, 0, (float)d.w, (float)d.h, d.round / 100.f, 0, 0, K_RECT, K_RECT, 1};
  }
  return root;
}

// ---------- public setters ----------
void setMood(Mood m, uint32_t holdMs) { if (m < MOOD_COUNT) { reqMood = m; reqHold = holdMs; reqMoodFlag = true; } }
void setBaseMood(Mood m) { if (m < MOOD_COUNT) base = m; }
Mood baseMood() { return base; }
void lookAt(float x, float y) { reqLookX = constrain(x, -1.f, 1.f); reqLookY = constrain(y, -1.f, 1.f); reqLookFlag = true; }
void setFx(Fx fx) { fxMode = fx; }
void setSleeping(bool on) { sleeping = on; }
void setShake(bool on) { shaking = on; }
void setStatus(Status s, int pct) { status = s; statusPct = pct; }
void playWakeUp() { wakeReq = true; }
void setCaption(bool on) { captionOn = on; }
void flash(bool on) { flashOn = on; }
Mood currentMood() { return mood; }

// ---------- weather particles ----------
static void spawn(Particle& p, Fx fx, bool initial) {
  p.x = random(20, LCD_W - 20);
  p.y = initial ? random(0, LCD_H) : -20;
  p.life = 1;
  p.vx = 0;
  lv_obj_remove_flag(p.o, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_style_opa(p.o, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(p.o, 2, 0);
  switch (fx) {
    case FX_RAIN: case FX_THUNDER:
      lv_obj_set_size(p.o, 3, 18);
      lv_obj_set_style_bg_color(p.o, lv_color_hex(0x4A90FF), 0);
      p.vy = random(380, 520); p.vx = -40;
      break;
    case FX_SNOW:
      lv_obj_set_size(p.o, 9, 9);
      lv_obj_set_style_radius(p.o, LV_RADIUS_CIRCLE, 0);
      lv_obj_set_style_bg_color(p.o, lv_color_hex(0xFFFFFF), 0);
      p.vy = random(40, 90); p.vx = random(-15, 15);
      break;
    case FX_CLOUD:
      lv_obj_set_size(p.o, random(90, 150), random(28, 40));
      lv_obj_set_style_radius(p.o, LV_RADIUS_CIRCLE, 0);
      lv_obj_set_style_bg_color(p.o, lv_color_hex(0x707880), 0);
      lv_obj_set_style_opa(p.o, LV_OPA_70, 0);
      p.y = random(20, 90); p.x = initial ? random(0, LCD_W) : -150;
      p.vx = random(8, 22); p.vy = 0;
      break;
    case FX_FOG:
      lv_obj_set_size(p.o, 300, 34);
      lv_obj_set_style_radius(p.o, LV_RADIUS_CIRCLE, 0);
      lv_obj_set_style_bg_color(p.o, lv_color_hex(0x909090), 0);
      lv_obj_set_style_opa(p.o, LV_OPA_30, 0);
      p.y = random(40, 440); p.x = initial ? random(-150, 300) : -300;
      p.vx = random(10, 25); p.vy = 0;
      break;
    case FX_HOT:
      lv_obj_set_size(p.o, 10, 14);
      lv_obj_set_style_radius(p.o, LV_RADIUS_CIRCLE, 0);
      lv_obj_set_style_bg_color(p.o, lv_color_hex(0x60C0FF), 0);
      p.x = (random(0, 2) ? CX + 120 : CX - 120) + random(-20, 20);
      p.y = EYE_Y + random(0, 40); p.vy = random(60, 110);
      break;
    case FX_SUN:
      lv_obj_set_size(p.o, 8, 8);
      lv_obj_set_style_radius(p.o, LV_RADIUS_CIRCLE, 0);
      lv_obj_set_style_bg_color(p.o, lv_color_hex(0xFFD040), 0);
      p.x = CX + random(-120, 120); p.y = random(30, 100);
      p.vy = -20; p.life = random(5, 10) / 10.f;
      break;
    default: lv_obj_add_flag(p.o, LV_OBJ_FLAG_HIDDEN); break;
  }
}

static void updateWeather(float dt, Fx fx) {
  if (fx != shownFx) {
    shownFx = fx;
    for (auto& p : parts) lv_obj_add_flag(p.o, LV_OBJ_FLAG_HIDDEN);
    int count = (fx == FX_CLOUD || fx == FX_FOG) ? 4 : (fx == FX_HOT) ? 3 : (fx == FX_SUN) ? 8 : (fx == FX_COLD) ? 5 : NP;
    if (fx != FX_NONE) for (int i = 0; i < count; i++) spawn(parts[i], fx == FX_COLD ? FX_SNOW : fx, true);
  }
  if (fx == FX_NONE) return;
  Fx kind = (fx == FX_COLD) ? FX_SNOW : fx;
  for (int i = 0; i < NP; i++) {
    Particle& p = parts[i];
    if (lv_obj_has_flag(p.o, LV_OBJ_FLAG_HIDDEN)) continue;
    p.x += p.vx * dt;
    p.y += p.vy * dt;
    if (kind == FX_SNOW) p.x += sinf(p.y * 0.05f + i) * 12 * dt;
    if (kind == FX_SUN) {
      p.life -= dt * 0.6f;
      lv_obj_set_style_opa(p.o, (lv_opa_t)(constrain(p.life, 0.f, 1.f) * 255), 0);
      if (p.life <= 0) spawn(p, kind, true);
    }
    if (p.y > LCD_H + 20 || p.x > LCD_W + 20 || p.x < -320) spawn(p, kind, false);
    lv_obj_set_pos(p.o, (int)p.x, (int)p.y);
  }
  if (fx == FX_THUNDER) {
    uint32_t dtms = (uint32_t)(dt * 1000);
    if (nextFlash > dtms) nextFlash -= dtms;
    else { nextFlash = random(3000, 9000); flashOpa = 255; }
  }
  if (flashOpa > 0) {
    flashOpa -= dt * 900;
    if (flashOpa <= 0) { flashOpa = 0; lv_obj_add_flag(flashObj, LV_OBJ_FLAG_HIDDEN); }
    else {
      lv_obj_remove_flag(flashObj, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_style_bg_opa(flashObj, (lv_opa_t)(flashOpa * 0.55f), 0);
    }
  }
}

// ---------- timing helpers ----------
static float approach(float cur, float target, float k) { return cur + (target - cur) * k; }

// Blink: 5 frames of 40 ms (open, closing, shut, opening, open) -> eye height factor.
static float blinkFactor(uint32_t dtMs, bool allowed) {
  static const float KEY[5] = {1.f, 0.45f, 0.07f, 0.45f, 1.f};
  if (blinkT > 0) {
    blinkT += dtMs;
    if (blinkT >= 200) {
      blinkT = 0;
      if (doubleBlink) { doubleBlink = false; blinkIn = 90; }
      else { blinkIn = random(BLINK_MIN_MS, BLINK_MAX_MS); doubleBlink = random(0, 100) < 20; }
      return 1.f;
    }
    float p = blinkT / 50.f;
    int i = (int)p;
    return KEY[i] + (KEY[i + 1] - KEY[i]) * (p - i);
  }
  if (!allowed) return 1.f;
  if (blinkIn > dtMs) blinkIn -= dtMs; else blinkT = 1;
  return 1.f;
}

// Look-around every 8-15 s: left / right / up / down, back to the centre after a moment.
static void autoLook(uint32_t dtMs) {
  if (nowMs < autoLookBlockUntil || sleeping) return;
  if (lookBackAt && nowMs > lookBackAt) { lookTX = lookTY = 0; lookBackAt = 0; neckEvtAt = nowMs + 150; }
  if (nextLook > dtMs) { nextLook -= dtMs; return; }
  nextLook = random(LOOK_MIN_MS, LOOK_MAX_MS);
  switch (random(0, 6)) {
    case 0: lookTX = -LOOK_PX_X; lookTY = 0; break;
    case 1: lookTX = LOOK_PX_X; lookTY = 0; break;
    case 2: lookTX = 0; lookTY = -LOOK_PX_Y; break;
    case 3: lookTX = 0; lookTY = LOOK_PX_Y; break;
    default: lookTX = random(-55, 56); lookTY = random(-20, 21); break;
  }
  lookBackAt = nowMs + random(1200, 2600);
  neckEvtAt = nowMs + 150;                         // the neck follows a little later
}

// ---------- main update ----------
void update(uint32_t dtMs) {
  if (!root) return;
  nowMs += dtMs;
  float dt = dtMs / 1000.f;
  t += dt;

  // 1. requests
  if (reqMoodFlag) { reqMoodFlag = false; mood = reqMood; holdLeft = reqHold; }
  else if (holdLeft > 0) { holdLeft = holdLeft > dtMs ? holdLeft - dtMs : 0; if (!holdLeft) mood = base; }
  if (reqLookFlag) {
    reqLookFlag = false;
    lookTX = reqLookX * LOOK_PX_X; lookTY = reqLookY * LOOK_PX_Y;
    autoLookBlockUntil = nowMs + 3000; lookBackAt = 0;
  }
  if (wakeReq) { wakeReq = false; wakeStep = 1; wakeT = 0; }

  // 2. pick what to show: wake-up > sleep > battery look (when resting) > mood
  const MoodDef* def = &MOODS[mood];
  bool resting = (mood == base) && holdLeft == 0;
  if (sleeping && !wakeStep) def = &LOOK_SLEEP;
  else if (resting && status == ST_CHARGING) def = &LOOK_CHARGE;
  else if (resting && status == ST_LOW_BATTERY) def = &LOOK_LOWBATT;
  EyeDef target[2] = {def->eye[0], def->eye[1]};
  uint32_t color = def->color;
  float tau = 70;                                  // easing time constant (ms)
  if (wakeStep) {
    wakeT += dtMs;
    if (wakeStep <= 5 && wakeT >= 120) { wakeStep++; wakeT = 0; }
    if (wakeStep == 6 && wakeT >= 900) wakeStep = 0;
    if (wakeStep) {
      target[0] = target[1] = WAKE_STEPS[wakeStep > 5 ? 4 : wakeStep - 1];
      color = CYAN;
      tau = 30;
    }
  }
  // weather tints a calm face
  Fx fx = fxMode;
  if (def == &MOODS[M_NORMAL] && !wakeStep) {
    if (fx == FX_SUN) color = 0xFFE070;
    else if (fx == FX_RAIN || fx == FX_THUNDER) color = 0x6090D0;
    else if (fx == FX_COLD) color = 0xA0E0FF;
    else if (fx == FX_HOT) color = 0xFF9060;
    else if (fx == FX_FOG || fx == FX_CLOUD) color = 0xB0B8C0;
  }
  if (flashOn) color = 0xFFFFFF;
  float k = 1.f - expf(-(float)dtMs / tau);

  // 3. look direction
  autoLook(dtMs);
  float tx = lookTX, ty = lookTY;
  if (def->lookX != FREE) tx = def->lookX / 100.f * LOOK_PX_X;
  if (def->lookY != FREE) ty = def->lookY / 100.f * LOOK_PX_Y;
  if (sleeping || wakeStep) { tx = 0; ty = 0; }
  lookX = approach(lookX, tx, k * 0.8f);
  lookY = approach(lookY, ty, k * 0.8f);
  if (neckEvtAt && nowMs >= neckEvtAt) {
    neckEvtAt = 0;
    events::publish(events::EV_LOOK, (int)(lookTX / LOOK_PX_X * 100), (int)(lookTY / LOOK_PX_Y * 100));
  }

  // 4. ease every eye towards its target; a new shape squashes the eye shut first
  bool anyRect = target[0].kind == K_RECT || target[1].kind == K_RECT;
  float blink = blinkFactor(dtMs, def->blink && anyRect && !wakeStep && !sleeping);
  for (int i = 0; i < 2; i++) {
    EyeState& s = es[i];
    const EyeDef& d = target[i];
    if (d.kind != s.kind) {
      if (wakeStep) { s.kind = d.kind; s.squash = 1; }
      else {
        s.squash -= dt / 0.09f;
        if (s.squash <= 0) { s.squash = 0; s.kind = d.kind; }
      }
    } else if (s.squash < 1) s.squash = fminf(1, s.squash + dt / 0.09f);
    s.x = approach(s.x, (i ? d.dx : -d.dx), k);
    s.y = approach(s.y, d.dy, k);
    s.w = approach(s.w, d.w, k);
    s.h = approach(s.h, d.h, k);
    s.round = approach(s.round, d.round / 100.f, k);
    s.lidIn = approach(s.lidIn, d.lidIn / 100.f, k);
    s.lidOut = approach(s.lidOut, d.lidOut / 100.f, k);
  }
  cr = approach(cr, (color >> 16) & 255, k);
  cg = approach(cg, (color >> 8) & 255, k);
  cb = approach(cb, color & 255, k);

  // 5. small life: breathing, jitter, spin
  spin += dt * 300;
  float breathe = 1.f + 0.015f * sinf(t * 2 * PI / 4.f);         // 4 s breathing
  float jitter = 0;
  if (shaking) jitter = sinf(t * 54) * 9;
  else if (mood == M_SCARED && !sleeping) jitter = sinf(t * 130) * 3;
  else if (fx == FX_COLD && !sleeping) jitter = sinf(t * 48) * 3;

  // 6. build the frame
  frame.color = ((uint32_t)cr << 16) | ((uint32_t)cg << 8) | (uint32_t)cb;
  frame.glow = sleeping ? 0 : FACE_GLOW;
  frame.t = t;
  frame.extras = wakeStep ? 0 : def->extras;
  frame.mouth = wakeStep ? MO_NONE : def->mouth;
  frame.mouthX = CX + lookX * 0.5f + jitter;
  frame.mouthY = MOUTH_Y + lookY * 0.5f;
  frame.percent = statusPct;
  frame.text = "";
  frame.textColor = 0x8B95A3;
  for (int i = 0; i < 2; i++) {
    EyeState& s = es[i];
    EyeFrame& e = frame.eye[i];
    int side = i ? 1 : -1;
    e.kind = s.kind;
    e.cx = CX + side * EYE_GAP / 2 + s.x + lookX + jitter;
    e.cy = EYE_Y + s.y + lookY + (mood == M_DIZZY ? sinf(t * 5 + i) * 5 : 0);
    float sq = s.squash;
    if (s.kind == K_RECT) {
      e.w = s.w * breathe;
      e.h = fmaxf(6, s.h * breathe * blink * sq);
      e.radius = fminf(e.w, s.h) * s.round;
    } else {
      e.w = fmaxf(8, s.w * breathe * (0.3f + 0.7f * sq));
      e.h = e.w;
      e.radius = 0;
    }
    e.lidIn = s.lidIn;
    e.lidOut = s.lidOut;
    e.spin = spin * side;
  }
  // talking mouth follows the voice level while Chiku speaks
  float lvl = audio::speechLevel();
  if (lvl >= 0 && !sleeping && !wakeStep && mood != M_ERROR && def->mouth != MO_BARS) {
    frame.mouth = MO_TALK;
    talk = approach(talk, lvl, 0.5f);
  } else talk = approach(talk, frame.mouth == MO_TALK ? 0.35f + 0.3f * sinf(t * 9) : 0, 0.3f);
  frame.mouthOpen = talk;
  if (frame.mouth == MO_BARS) {                    // listening: bars follow the microphone
    float m = mic::online() ? mic::level() : 0.25f + 0.2f * sinf(t * 3);
    for (int i = 0; i < 9; i++) {
      float shape = 1.f - fabsf(i - 4) / 5.f;
      frame.bars[i] = constrain(m * shape * (0.7f + 0.3f * sinf(t * 11 + i * 1.7f)), 0.f, 1.f);
    }
    frame.text = "Listening...";
    frame.textColor = 0x00E5FF;
  }
  if (def == &LOOK_LOWBATT) {
    snprintf(textBuf, sizeof(textBuf), "Battery %d%% \xE2\x80\x94 feed me!", statusPct);
    frame.text = textBuf;
    frame.textColor = 0xFFA62B;
  } else if (def == &LOOK_CHARGE) {
    frame.text = "Charging";
    frame.textColor = 0x4ADE80;
  } else if (def == &LOOK_SLEEP) {
    frame.text = "Tap to wake me";
    frame.textColor = 0x3A3F4A;
  }
  frame.caption = "";
  if (captionOn) {
    const char* n = MOODS[mood].name;
    size_t j = 0;
    for (; n[j] && j < sizeof(captionBuf) - 1; j++) captionBuf[j] = toupper(n[j]);
    captionBuf[j] = 0;
    frame.caption = captionBuf;
  }

  // 7. redraw only what changed: old areas + new areas
  lv_area_t areas[16];
  int n = haveLast ? frameAreas(lastFrame, areas, 8) : 0;
  n += frameAreas(frame, areas + n, 8);
  if (!haveLast) lv_obj_invalidate(root);
  for (int i = 0; i < n; i++) lv_obj_invalidate_area(root, &areas[i]);
  lastFrame = frame;
  haveLast = true;

  updateWeather(dt, fx);
}

}  // namespace face
