// face.cpp - see face.h
#include "face.h"
#include "config.h"
#include "events.h"
#include "motion.h"
#include <math.h>

namespace face {

// ---------- mood table ----------
struct MoodDef {
  float w, h;        // eye size in px
  float round;       // corner radius as fraction of the short side (0.5 = pill)
  float topCover;    // 0..1 of eye height covered by the upper lid
  float topAng;      // lid tilt in degrees, + = inner side lower (angry)
  float botCover;    // 0..1 covered from below (happy cheeks)
  float yShift;      // move eyes down (+)
  uint32_t color;    // 0xRRGGBB
  float spiral;      // 1 = dizzy spiral
  float heart;       // 1 = heart eyes
  float lookX, lookY; // forced look (999 = free)
};

static const MoodDef MOODS[MOOD_COUNT] = {
  /* NORMAL    */ {110, 150, 0.50f, 0.00f,   0, 0.00f,   0, 0x5CE1FF, 0, 0, 999, 999},
  /* HAPPY     */ {112, 140, 0.50f, 0.00f,   0, 0.48f, -10, 0x7CFFB0, 0, 0, 999, 999},
  /* SAD       */ {108, 140, 0.50f, 0.38f, -22, 0.00f,  20, 0x6A8CFF, 0, 0, 0, 0.5f},
  /* SLEEPY    */ {112, 120, 0.50f, 0.60f,   0, 0.00f,  25, 0x8AA0B0, 0, 0, 0, 0.6f},
  /* ANGRY     */ {112, 140, 0.35f, 0.40f,  24, 0.00f,   5, 0xFF4030, 0, 0, 999, 999},
  /* SURPRISED */ {128, 184, 0.50f, 0.00f,   0, 0.00f,  -5, 0xFFFFFF, 0, 0, 0, -0.2f},
  /* DIZZY     */ {124, 124, 0.50f, 0.00f,   0, 0.00f,   0, 0xFFE060, 1, 0, 0, 0},
  /* LOVE      */ {110, 110, 0.50f, 0.00f,   0, 0.00f,   0, 0xFF5C9A, 0, 1, 0, 0},
  /* THINKING  */ {104, 136, 0.45f, 0.22f,   9, 0.00f,   0, 0xC080FF, 0, 0, 0.7f, -0.7f},
  /* LISTENING */ {124, 172, 0.50f, 0.00f,   0, 0.00f,  -8, 0x40FFD0, 0, 0, 0, 0},
  /* JEALOUS   */ {104, 128, 0.45f, 0.34f,  14, 0.00f,   6, 0x60D060, 0, 0, 0.9f, 0.05f},   // green side-eye
  /* SULK      */ { 98, 118, 0.50f, 0.46f, -10, 0.00f,  22, 0x7080A0, 0, 0, -0.9f, 0.5f},   // ruthna: looks away
  /* SHY       */ { 94, 118, 0.50f, 0.12f,  -8, 0.30f,  16, 0xFFA0C0, 0, 0, -0.6f, 0.7f},   // + blush
  /* EXCITED   */ {132, 150, 0.50f, 0.00f,   0, 0.50f, -12, 0xFFE060, 0, 0, 999, 999},      // + sparkles
  /* SCARED    */ {116, 176, 0.50f, 0.00f,   0, 0.00f,   0, 0xE0E8FF, 0, 0, 0, -0.1f},      // + trembling
  /* CRY       */ {110, 134, 0.50f, 0.36f, -22, 0.00f,  24, 0x70A0FF, 0, 0, 0, 0.45f},      // + tears
};

static const char* MOOD_NAMES[MOOD_COUNT] = {
  "normal", "happy", "sad", "sleepy", "angry", "surprised", "dizzy", "love", "thinking", "listening",
  "jealous", "sulk", "shy", "excited", "scared", "cry"};
const char* moodName(Mood m) { return m < MOOD_COUNT ? MOOD_NAMES[m] : "?"; }

// ---------- state ----------
static const int CX = LCD_W / 2;
static const int EYE_CY = 205;
static const int EYE_GAP = 190;      // distance between eye centres

struct EyeObjs {
  lv_obj_t* body;
  lv_obj_t* lidTop;
  lv_obj_t* lidBot;
  lv_obj_t* arc;        // spiral
  lv_obj_t* heart[3];
};
static EyeObjs eyes[2];               // 0 = left, 1 = right

// Current (eased) values
static float cw = 110, ch = 150, cround = 0.5f, ctop = 0, cang = 0, cbot = 0, cy = 0;
static float cr = 92, cg = 225, cb = 255, cspiral = 0, cheart = 0;
static float lookX = 0, lookY = 0, lookTX = 0, lookTY = 0;

// Requests from other tasks
static volatile Mood reqMood = M_NORMAL;
static volatile bool reqMoodFlag = false;
static volatile uint32_t reqHold = 0;
static volatile Mood baseMood = M_NORMAL;
static volatile Fx fxMode = FX_NONE;
static volatile bool sleeping = false, shaking = false;
static volatile uint32_t autoLookBlockUntil = 0;

static Mood mood = M_NORMAL;
static uint32_t holdLeft = 0;
static uint32_t blinkIn = 2500, blinkT = 0;       // ms until blink / blink progress
static uint32_t lookIn = 2000;
static float spin = 0, pulse = 0;
static lv_obj_t* root = nullptr;

// ---------- particles (weather + mood effects) ----------
struct Particle { lv_obj_t* o; float x, y, vx, vy, life; };
static const int NP = 14;
static Particle parts[NP];
static lv_obj_t* flashObj = nullptr;

// Mood effects: tears, blush, sparkles, "z z z"
struct MoodFx {
  lv_obj_t* tear[2];
  lv_obj_t* blush[2];
  lv_obj_t* spark[4];
  lv_obj_t* zz[3];
  float tearY[2], sparkLife[4], sparkX[4], sparkY[4], zLife[3], blushAmt;
};
static MoodFx mf;
static float eyeCx[2] = {CX - 95, CX + 95}, eyeCy[2] = {EYE_CY, EYE_CY}, eyeW[2] = {110, 110}, eyeH[2] = {150, 150};
static Fx shownFx = FX_NONE;
static uint32_t nextFlash = 4000;
static float flashOpa = 0;
static uint32_t winkMs = 0, winkDur = 0, rageMs = 0;
static int winkSide = 1;

static lv_obj_t* rect(lv_obj_t* parent) {
  lv_obj_t* o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_remove_flag(o, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
  return o;
}

static lv_obj_t* circle(lv_obj_t* parent, int d, uint32_t color) {
  lv_obj_t* o = rect(parent);
  lv_obj_set_size(o, d, d);
  lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
  return o;
}

static float approach(float cur, float target, float k) { return cur + (target - cur) * k; }

lv_obj_t* create(lv_obj_t* parent) {
  root = rect(parent);
  lv_obj_set_size(root, LCD_W, LCD_H);
  lv_obj_set_pos(root, 0, 0);
  lv_obj_set_style_bg_color(root, lv_color_black(), 0);

  for (int i = 0; i < 2; i++) {
    EyeObjs& e = eyes[i];
    e.body = rect(root);
    lv_obj_set_style_clip_corner(e.body, true, 0);
    e.lidTop = rect(e.body);
    lv_obj_set_style_bg_color(e.lidTop, lv_color_black(), 0);
    e.lidBot = circle(e.body, 100, 0x000000);
    e.arc = lv_arc_create(e.body);
    lv_obj_remove_style(e.arc, nullptr, LV_PART_KNOB);
    lv_obj_remove_flag(e.arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_opa(e.arc, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_arc_color(e.arc, lv_color_black(), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(e.arc, 10, LV_PART_INDICATOR);
    lv_arc_set_bg_angles(e.arc, 0, 360);
    lv_arc_set_angles(e.arc, 0, 300);
    lv_obj_add_flag(e.arc, LV_OBJ_FLAG_HIDDEN);
    for (int h = 0; h < 3; h++) {
      e.heart[h] = (h < 2) ? circle(root, 40, 0xFF5C9A) : rect(root);
      if (h == 2) lv_obj_set_style_radius(e.heart[h], 6, 0);
      lv_obj_add_flag(e.heart[h], LV_OBJ_FLAG_HIDDEN);
    }
  }
  for (int i = 0; i < NP; i++) {
    parts[i].o = rect(root);
    lv_obj_add_flag(parts[i].o, LV_OBJ_FLAG_HIDDEN);
  }
  for (int i = 0; i < 2; i++) {
    mf.tear[i] = circle(root, 14, 0x70B0FF);
    lv_obj_add_flag(mf.tear[i], LV_OBJ_FLAG_HIDDEN);
    mf.blush[i] = circle(root, 44, 0xFF6090);
    lv_obj_set_style_bg_opa(mf.blush[i], LV_OPA_TRANSP, 0);
    mf.tearY[i] = i * 35;
  }
  for (int i = 0; i < 4; i++) {
    mf.spark[i] = rect(root);
    lv_obj_set_size(mf.spark[i], 12, 12);
    lv_obj_set_style_bg_color(mf.spark[i], lv_color_hex(0xFFF0A0), 0);
    lv_obj_set_style_transform_pivot_x(mf.spark[i], 6, 0);
    lv_obj_set_style_transform_pivot_y(mf.spark[i], 6, 0);
    lv_obj_set_style_transform_rotation(mf.spark[i], 450, 0);
    lv_obj_add_flag(mf.spark[i], LV_OBJ_FLAG_HIDDEN);
    mf.sparkLife[i] = i * 0.25f;
  }
  for (int i = 0; i < 3; i++) {
    mf.zz[i] = lv_label_create(root);
    lv_label_set_text(mf.zz[i], "z");
    lv_obj_set_style_text_font(mf.zz[i], &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(mf.zz[i], lv_color_hex(0x9AB0D0), 0);
    lv_obj_add_flag(mf.zz[i], LV_OBJ_FLAG_HIDDEN);
    mf.zLife[i] = i * 0.33f;
  }
  flashObj = rect(root);
  lv_obj_set_size(flashObj, LCD_W, LCD_H);
  lv_obj_set_style_bg_color(flashObj, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(flashObj, LV_OPA_TRANSP, 0);
  lv_obj_add_flag(flashObj, LV_OBJ_FLAG_HIDDEN);
  return root;
}

// ---------- public setters ----------
// Every feeling also moves the body: mood -> gait + cycles. A behaviour that sends its own gait
// right after setMood() simply overrides this one (it is processed later).
static void moodBody(Mood m) {
  static Mood last = MOOD_COUNT;
  static uint32_t lastMs = 0;
  uint32_t now = millis();
  if (m == last && now - lastMs < 3000) return;          // same mood re-requested: do not restart
  last = m; lastMs = now;
  motion::Gait g; uint8_t cyc = 3;
  switch (m) {
    case M_HAPPY: g = motion::G_HAPPY; cyc = 6; break;
    case M_EXCITED: g = motion::G_EXCITED; cyc = 5; break;
    case M_ANGRY: g = motion::G_ANGRY; break;
    case M_SCARED: g = motion::G_SCARED; cyc = 10; break;
    case M_SULK: g = motion::G_SULK; cyc = 2; break;
    case M_JEALOUS: g = motion::G_JEALOUS; cyc = 4; break;
    case M_SHY: g = motion::G_SHY; cyc = 2; break;
    case M_LOVE: g = motion::G_LOVE; cyc = 2; break;
    case M_SAD: case M_CRY: case M_SLEEPY: g = motion::G_SAD; cyc = 2; break;
    case M_SURPRISED: g = motion::G_SURPRISED; cyc = 1; break;
    case M_DIZZY: g = motion::G_WIGGLE; cyc = 8; break;
    case M_THINKING: g = motion::G_TILT_L; cyc = 1; break;
    default: return;                                     // normal / listening: leave the body alone
  }
  events::publish(events::EV_MOTION_CMD, g, cyc);
}

void setMood(Mood m, uint32_t holdMs) { reqMood = m; reqHold = holdMs; reqMoodFlag = true; moodBody(m); }
void setBaseMood(Mood m) { baseMood = m; }
void lookAt(float x, float y) {
  lookTX = constrain(x, -1.f, 1.f);
  lookTY = constrain(y, -1.f, 1.f);
  autoLookBlockUntil = millis() + 3000;
}
void setFx(Fx fx) { fxMode = fx; }
void wink(int side) { winkSide = side; winkDur = 520; winkMs = 1; }
void rage(uint32_t ms) { rageMs = ms; }
void setSleeping(bool on) { sleeping = on; }
void setShake(bool on) { shaking = on; }
Mood currentMood() { return mood; }

// ---------- heart drawing ----------
static void placeHeart(EyeObjs& e, float cx, float cy0, float size, bool show) {
  for (int h = 0; h < 3; h++) {
    if (show) lv_obj_remove_flag(e.heart[h], LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(e.heart[h], LV_OBJ_FLAG_HIDDEN);
  }
  if (!show) return;
  float a = size * 0.6f;
  int d = (int)a;
  uint32_t col = ((int)cr << 16) | ((int)cg << 8) | (int)cb;
  for (int h = 0; h < 3; h++) lv_obj_set_style_bg_color(e.heart[h], lv_color_hex(col), 0);
  lv_obj_set_size(e.heart[0], d, d);
  lv_obj_set_size(e.heart[1], d, d);
  lv_obj_set_pos(e.heart[0], (int)(cx - 0.354f * a - a / 2), (int)(cy0 - 0.354f * a - a / 2));
  lv_obj_set_pos(e.heart[1], (int)(cx + 0.354f * a - a / 2), (int)(cy0 - 0.354f * a - a / 2));
  lv_obj_set_size(e.heart[2], d, d);
  lv_obj_set_pos(e.heart[2], (int)(cx - a / 2), (int)(cy0 - a / 2));
  lv_obj_set_style_transform_pivot_x(e.heart[2], d / 2, 0);
  lv_obj_set_style_transform_pivot_y(e.heart[2], d / 2, 0);
  lv_obj_set_style_transform_rotation(e.heart[2], 450, 0);
}

// ---------- weather / mood particles ----------
static void hideParticles() {
  for (auto& p : parts) lv_obj_add_flag(p.o, LV_OBJ_FLAG_HIDDEN);
}

static void spawn(Particle& p, Fx fx, bool initial) {
  p.x = random(20, LCD_W - 20);
  p.y = initial ? random(0, LCD_H) : -20;
  p.life = 1;
  p.vx = 0;
  lv_obj_remove_flag(p.o, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_style_opa(p.o, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(p.o, 2, 0);
  switch (fx) {
    case FX_RAIN:
    case FX_THUNDER:
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
      p.y = EYE_CY + random(0, 40); p.vy = random(60, 110);
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

static void updateFx(float dt) {
  Fx fx = fxMode;
  if (fx != shownFx) {
    shownFx = fx;
    hideParticles();
    int count = (fx == FX_CLOUD) ? 4 : (fx == FX_FOG) ? 4 : (fx == FX_HOT) ? 3 : (fx == FX_SUN) ? 8 : NP;
    if (fx != FX_NONE && fx != FX_COLD) {
      for (int i = 0; i < count; i++) spawn(parts[i], fx, true);
    }
    if (fx == FX_COLD) {   // cold = a few snowflakes + blue tint (tint is in update())
      for (int i = 0; i < 5; i++) spawn(parts[i], FX_SNOW, true);
    }
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
    bool out = p.y > LCD_H + 20 || p.x > LCD_W + 20 || p.x < -320;
    if (out) spawn(p, kind, false);
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

// ---------- mood effects ----------
static void updateMoodFx(float dt, Mood m) {
  bool cry = (m == M_CRY), sad = (m == M_SAD) || cry;
  bool sleepy = (m == M_SLEEPY) || sleeping;
  bool excited = (m == M_EXCITED);
  // tears: a drop slides down from under each eye
  for (int i = 0; i < 2; i++) {
    if (!sad) { lv_obj_add_flag(mf.tear[i], LV_OBJ_FLAG_HIDDEN); continue; }
    mf.tearY[i] += dt * (cry ? 140.f : 70.f);
    if (mf.tearY[i] > 90) mf.tearY[i] = 0;
    float x = eyeCx[i] + (i == 0 ? -1 : 1) * eyeW[i] * 0.2f;
    float y = eyeCy[i] + eyeH[i] * 0.45f + mf.tearY[i];
    lv_obj_remove_flag(mf.tear[i], LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(mf.tear[i], (int)x - 7, (int)y);
    lv_obj_set_style_opa(mf.tear[i], (lv_opa_t)(255 * (1.f - mf.tearY[i] / 95.f)), 0);
  }
  // blush: pink cheeks fade in for shy / love / a little for happy
  float tb = (m == M_SHY) ? 1.f : (m == M_LOVE) ? 0.7f : (m == M_HAPPY || excited) ? 0.3f : 0.f;
  mf.blushAmt += (tb - mf.blushAmt) * fminf(1.f, dt * 6);
  for (int i = 0; i < 2; i++) {
    lv_obj_set_pos(mf.blush[i], (int)(eyeCx[i] + (i == 0 ? -1 : 1) * 20 - 22), (int)(eyeCy[i] + eyeH[i] * 0.5f + 8));
    lv_obj_set_size(mf.blush[i], 52, 30);
    lv_obj_set_style_bg_opa(mf.blush[i], (lv_opa_t)(mf.blushAmt * 150), 0);
  }
  // sparkles: little stars pop around the eyes when excited
  for (int i = 0; i < 4; i++) {
    if (!excited) { lv_obj_add_flag(mf.spark[i], LV_OBJ_FLAG_HIDDEN); continue; }
    mf.sparkLife[i] += dt * 1.6f;
    if (mf.sparkLife[i] > 1.f) {
      mf.sparkLife[i] = 0;
      int side = (i & 1) ? 1 : -1;
      mf.sparkX[i] = CX + side * random(60, 190);
      mf.sparkY[i] = EYE_CY + random(-110, 60);
    }
    lv_obj_remove_flag(mf.spark[i], LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(mf.spark[i], (int)mf.sparkX[i], (int)mf.sparkY[i]);
    float s = sinf(3.14159f * mf.sparkLife[i]);
    lv_obj_set_style_opa(mf.spark[i], (lv_opa_t)(255 * s), 0);
    lv_obj_set_style_transform_scale(mf.spark[i], (int)(80 + 200 * s), 0);
  }
  // z z z: rise from the right eye while sleepy
  for (int i = 0; i < 3; i++) {
    if (!sleepy) { lv_obj_add_flag(mf.zz[i], LV_OBJ_FLAG_HIDDEN); continue; }
    mf.zLife[i] += dt * 0.35f;
    if (mf.zLife[i] > 1.f) mf.zLife[i] = 0;
    lv_obj_remove_flag(mf.zz[i], LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(mf.zz[i], (int)(CX + 120 + mf.zLife[i] * 50 + i * 12), (int)(EYE_CY - 40 - mf.zLife[i] * 120));
    lv_obj_set_style_text_opa(mf.zz[i], (lv_opa_t)(255 * sinf(3.14159f * mf.zLife[i])), 0);
  }
}

// ---------- main update ----------
void update(uint32_t dtMs) {
  if (!root) return;
  float dt = dtMs / 1000.f;
  float k = 1.f - expf(-(float)dtMs / 90.f);          // easing speed

  // 0. rage: angry face that shakes, then back to normal
  if (rageMs) {
    rageMs = rageMs > dtMs ? rageMs - dtMs : 0;
    mood = M_ANGRY;
    holdLeft = 400;
    shaking = rageMs > 0;
    if (!rageMs) mood = baseMood;
  }
  // 1. pick up mood request
  if (reqMoodFlag) {
    reqMoodFlag = false;
    mood = reqMood;
    holdLeft = reqHold;
  } else if (holdLeft > 0) {
    holdLeft = (holdLeft > dtMs) ? holdLeft - dtMs : 0;
    if (holdLeft == 0) mood = baseMood;
  }
  MoodDef t = MOODS[sleeping ? M_SLEEPY : mood];
  if (sleeping) { t.topCover = 0.92f; t.h = 100; t.color = 0x203040; }

  // weather tints the eye colour
  uint32_t color = t.color;
  Fx fx = fxMode;
  if (!sleeping && (mood == M_NORMAL || mood == M_HAPPY)) {
    if (fx == FX_SUN) { color = 0xFFE070; t.botCover = 0.3f; }
    else if (fx == FX_RAIN || fx == FX_THUNDER) { color = 0x6090D0; t.topCover = 0.18f; }
    else if (fx == FX_COLD) color = 0xA0E0FF;
    else if (fx == FX_HOT) { color = 0xFF9060; t.topCover = 0.3f; }
    else if (fx == FX_FOG || fx == FX_CLOUD) color = 0xB0B8C0;
  }

  // 2. look direction (auto look when idle)
  if (lv_tick_get() > autoLookBlockUntil && !sleeping) {
    if (lookIn > dtMs) lookIn -= dtMs;
    else {
      lookIn = random(1200, 3800);
      lookTX = random(0, 100) < 30 ? 0 : random(-80, 81) / 100.f;
      lookTY = random(-30, 31) / 100.f;
      events::publish(events::EV_LOOK, (int)(lookTX * 100), (int)(lookTY * 100));
    }
  }
  float fx_ = (t.lookX < 900) ? t.lookX : lookTX;
  float fy_ = (t.lookY < 900) ? t.lookY : lookTY;
  if (sleeping) { fx_ = 0; fy_ = 0.3f; }
  lookX = approach(lookX, fx_, k * 0.8f);
  lookY = approach(lookY, fy_, k * 0.8f);

  // 3. ease all parameters
  cw = approach(cw, t.w, k);   ch = approach(ch, t.h, k);
  cround = approach(cround, t.round, k);
  ctop = approach(ctop, t.topCover, k);  cang = approach(cang, t.topAng, k);
  cbot = approach(cbot, t.botCover, k);  cy = approach(cy, t.yShift, k);
  cr = approach(cr, (color >> 16) & 255, k);
  cg = approach(cg, (color >> 8) & 255, k);
  cb = approach(cb, color & 255, k);
  cspiral = approach(cspiral, t.spiral, k);
  cheart = approach(cheart, t.heart, k);

  // 4. blink
  float blink = 0;
  bool canBlink = !(mood == M_DIZZY || mood == M_LOVE) || sleeping;
  if (blinkT > 0) {
    blinkT += dtMs;
    uint32_t dur = (mood == M_SLEEPY || sleeping) ? 320 : 170;
    if (blinkT >= dur) { blinkT = 0; blinkIn = random(2200, 5500); }
    else blink = sinf(3.14159f * blinkT / (float)dur);
  } else if (canBlink) {
    if (blinkIn > dtMs) blinkIn -= dtMs;
    else blinkT = 1;
  }

  if (winkMs) { winkMs += dtMs; if (winkMs >= winkDur) winkMs = 0; }
  // 5. extras: pulse, spin, jitter
  pulse += dt * 6;
  spin += dt * 420;
  float breathe = 1.f + 0.015f * sinf(pulse * 0.5f);
  float heartPulse = 1.f + 0.12f * sinf(pulse * 1.6f);
  float jitterX = 0;
  if (shaking) jitterX = sinf(pulse * 9) * 9;
  else if (mood == M_SCARED && !sleeping) jitterX = sinf(pulse * 22) * 4;
  else if (fx == FX_COLD && !sleeping) jitterX = sinf(pulse * 8) * 3;
  float lis = (mood == M_LISTENING) ? 1.f + 0.05f * sinf(pulse * 1.3f) : 1.f;

  // 6. apply to every eye object
  for (int i = 0; i < 2; i++) {
    EyeObjs& e = eyes[i];
    int side = i == 0 ? -1 : 1;
    float w = cw * breathe * lis;
    float bl = blink;
    if (winkMs && i == (winkSide > 0 ? 1 : 0)) bl = fmaxf(bl, sinf(3.14159f * fminf(1.f, (float)winkMs / winkDur)));   // wink: one lid closes
    float h = ch * breathe * lis * (1.f - 0.92f * bl);
    if (lookX * side > 0.1f) w *= 1.f - 0.12f * fabsf(lookX);     // far-looking eye narrower
    if (mood == M_THINKING && side == 1) { w *= 0.85f; h *= 0.85f; }
    if (h < 6) h = 6;
    float ecx = CX + side * EYE_GAP / 2 + lookX * 28 + jitterX;
    float ecy = EYE_CY + cy + lookY * 22 + (mood == M_DIZZY ? sinf(pulse) * 6 : 0);
    eyeCx[i] = ecx; eyeCy[i] = ecy; eyeW[i] = w; eyeH[i] = h;
    int iw = (int)w, ih = (int)h;
    lv_obj_set_size(e.body, iw, ih);
    lv_obj_set_pos(e.body, (int)(ecx - w / 2), (int)(ecy - h / 2));
    lv_obj_set_style_radius(e.body, (int)(fminf(w, h) * cround), 0);
    lv_obj_set_style_bg_color(e.body, lv_color_make((uint8_t)cr, (uint8_t)cg, (uint8_t)cb), 0);
    lv_obj_set_style_bg_opa(e.body, cheart > 0.5f ? LV_OPA_TRANSP : LV_OPA_COVER, 0);

    // upper lid: wide black rectangle, tilted
    int lw = iw + 90;
    lv_obj_set_size(e.lidTop, lw, ih);
    lv_obj_set_pos(e.lidTop, -45, (int)(ctop * ih) - ih);
    lv_obj_set_style_transform_pivot_x(e.lidTop, lw / 2, 0);
    lv_obj_set_style_transform_pivot_y(e.lidTop, ih / 2, 0);
    lv_obj_set_style_transform_rotation(e.lidTop, (int)(-side * cang * 10), 0);
    // lower lid: big black circle rising from below
    int d = (int)(w * 1.7f);
    lv_obj_set_size(e.lidBot, d, d);
    lv_obj_set_pos(e.lidBot, (iw - d) / 2, (int)(ih - cbot * ih));

    // spiral for dizzy
    if (cspiral > 0.5f) {
      lv_obj_remove_flag(e.arc, LV_OBJ_FLAG_HIDDEN);
      int ad = (int)(fminf(w, h) * 0.72f);
      lv_obj_set_size(e.arc, ad, ad);
      lv_obj_set_pos(e.arc, (iw - ad) / 2, (ih - ad) / 2);
      lv_arc_set_rotation(e.arc, ((int)spin * (side == -1 ? 1 : -1)) % 360);
    } else {
      lv_obj_add_flag(e.arc, LV_OBJ_FLAG_HIDDEN);
    }
    placeHeart(e, ecx, ecy, 105 * heartPulse, cheart > 0.5f);
  }

  updateFx(dt);
  updateMoodFx(dt, sleeping ? M_SLEEPY : mood);
}

}  // namespace face
