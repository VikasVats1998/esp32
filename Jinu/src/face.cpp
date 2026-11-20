// face.cpp - see face.h
#include "face.h"
#include "config.h"
#include "display.h"
#include <math.h>

namespace face {

struct MoodDef {
  float w, h;          // eye size (px)
  float round;         // corner radius, fraction of the short side
  float topCover;      // 0..1 upper lid
  float topAng;        // lid tilt in degrees (+ = inner side lower = angry)
  float botCover;      // 0..1 cheeks from below
  float yShift;
  uint32_t color;      // 0xRRGGBB
  float spiral, heart;
  float lookX, lookY;  // forced look (999 = free)
};

// Sizes are for the 320x172 screen.
static const MoodDef MOODS[MOOD_COUNT] = {
  /* NORMAL    */ {64, 88, 0.50f, 0.00f,   0, 0.00f,   0, 0x5CE1FF, 0, 0, 999, 999},
  /* HAPPY     */ {66, 82, 0.50f, 0.00f,   0, 0.48f,  -6, 0x7CFFB0, 0, 0, 999, 999},
  /* SAD       */ {62, 82, 0.50f, 0.38f, -22, 0.00f,  10, 0x6A8CFF, 0, 0, 0, 0.5f},
  /* SLEEPY    */ {66, 70, 0.50f, 0.60f,   0, 0.00f,  14, 0x8AA0B0, 0, 0, 0, 0.6f},
  /* ANGRY     */ {66, 82, 0.35f, 0.40f,  24, 0.00f,   3, 0xFF4030, 0, 0, 999, 999},
  /* SURPRISED */ {74, 108, 0.50f, 0.00f,   0, 0.00f,  -3, 0xFFFFFF, 0, 0, 0, -0.2f},
  /* DIZZY     */ {72, 72, 0.50f, 0.00f,   0, 0.00f,   0, 0xFFE060, 1, 0, 0, 0},
  /* LOVE      */ {64, 64, 0.50f, 0.00f,   0, 0.00f,   0, 0xFF5C9A, 0, 1, 0, 0},
  /* THINKING  */ {60, 80, 0.45f, 0.22f,   9, 0.00f,   0, 0xC080FF, 0, 0, 0.7f, -0.7f},
  /* LISTENING */ {72, 100, 0.50f, 0.00f,   0, 0.00f,  -4, 0x40FFD0, 0, 0, 0, 0},
  /* JEALOUS   */ {60, 76, 0.45f, 0.34f,  14, 0.00f,   4, 0x60D060, 0, 0, 0.9f, 0.05f},
  /* SULK      */ {58, 70, 0.50f, 0.46f, -10, 0.00f,  13, 0x7080A0, 0, 0, -0.9f, 0.5f},
  /* SHY       */ {56, 70, 0.50f, 0.12f,  -8, 0.30f,   9, 0xFFA0C0, 0, 0, -0.6f, 0.7f},
  /* EXCITED   */ {76, 88, 0.50f, 0.00f,   0, 0.50f,  -7, 0xFFE060, 0, 0, 999, 999},
  /* SCARED    */ {68, 102, 0.50f, 0.00f,   0, 0.00f,   0, 0xE0E8FF, 0, 0, 0, -0.1f},
  /* CRY       */ {64, 78, 0.50f, 0.36f, -22, 0.00f,  14, 0x70A0FF, 0, 0, 0, 0.45f},
};
static const char* NAMES[MOOD_COUNT] = {"normal", "happy", "sad", "sleepy", "angry", "surprised", "dizzy",
  "love", "thinking", "listening", "jealous", "sulk", "shy", "excited", "scared", "cry"};
const char* moodName(Mood m) { return m < MOOD_COUNT ? NAMES[m] : "?"; }

static const int CX = LCD_W / 2, EYE_CY = 80, EYE_GAP = 112;

// current (eased) values
static float cw = 64, ch = 88, cround = .5f, ctop = 0, cang = 0, cbot = 0, cy = 0;
static float cr = 92, cg = 225, cb = 255, cspiral = 0, cheart = 0, cblush = 0;
static float lookX = 0, lookY = 0, lookTX = 0, lookTY = 0;
static float eyeX[2] = {CX - 56, CX + 56}, eyeY[2] = {EYE_CY, EYE_CY}, eyeW[2] = {64, 64}, eyeH[2] = {88, 88};

// requests from other tasks
static volatile Mood reqMood = M_NORMAL, baseMood = M_NORMAL;
static volatile bool reqFlag = false, sleeping = false;
static volatile uint32_t reqHold = 0, autoLookBlock = 0;

static Mood mood = M_NORMAL;
static uint32_t holdLeft = 0, blinkIn = 2500, blinkT = 0, lookIn = 2000;
static float pulse = 0, spin = 0;
static float tearY[2] = {0, 30}, spark[4] = {0, .25f, .5f, .75f}, sparkX[4], sparkYp[4], zLife[3] = {0, .33f, .66f};

static volatile int testPattern = 0;
void setTestPattern(int p) { testPattern = p; }
void init() {}
void setMood(Mood m, uint32_t hold) { reqMood = m; reqHold = hold; reqFlag = true; }
void setBaseMood(Mood m) { baseMood = m; }
void setSleeping(bool on) { sleeping = on; }
Mood currentMood() { return mood; }
void lookAt(float x, float y) { lookTX = constrain(x, -1.f, 1.f); lookTY = constrain(y, -1.f, 1.f); autoLookBlock = millis() + 3000; }

static float approach(float c, float t, float k) { return c + (t - c) * k; }

// ---------- drawing ----------
static uint16_t col(float r, float g, float b) { return display::canvas()->color565((uint8_t)r, (uint8_t)g, (uint8_t)b); }

static void drawHeart(Arduino_GFX* g, float x, float y, float s, uint16_t c) {
  g->fillCircle(x - s * 0.26f, y - s * 0.12f, s * 0.30f, c);
  g->fillCircle(x + s * 0.26f, y - s * 0.12f, s * 0.30f, c);
  g->fillTriangle(x - s * 0.53f, y - s * 0.02f, x + s * 0.53f, y - s * 0.02f, x, y + s * 0.5f, c);
}

static void drawEye(Arduino_GFX* g, int side, float cx, float cyy, float w, float h, uint16_t c) {
  int x0 = (int)(cx - w / 2), y0 = (int)(cyy - h / 2), iw = (int)w, ih = (int)h;
  if (cheart > 0.5f) { drawHeart(g, cx, cyy, 70 * (1.f + 0.12f * sinf(pulse * 1.6f)), c); return; }
  g->fillRoundRect(x0, y0, iw, ih, (int)(fminf(w, h) * cround), c);
  // upper lid: black columns down to a (tilted) line
  float slope = -side * tanf(cang * DEG_TO_RAD);
  if (ctop > 0.01f) {
    for (int x = x0; x < x0 + iw; x++) {
      int edge = (int)(y0 + ctop * ih + slope * (x - cx));
      if (edge > y0) g->drawFastVLine(x, y0 - 1, min(edge, y0 + ih) - y0 + 1, 0);
    }
  }
  // lower lid: black cap rising from below (cheeks)
  if (cbot > 0.01f) {
    for (int x = x0; x < x0 + iw; x++) {
      float d = (x - cx) / (w / 2);
      int edge = (int)(y0 + ih - cbot * ih * (1.f - 0.55f * d * d));
      if (edge < y0 + ih) g->drawFastVLine(x, edge, y0 + ih - edge + 1, 0);
    }
  }
  // dizzy spiral
  if (cspiral > 0.5f) {
    float rmax = fminf(w, h) * 0.42f, a0 = spin * (side == -1 ? 1 : -1) * DEG_TO_RAD, px = cx, py = cyy;
    for (float t = 0.3f; t < 12.5f; t += 0.35f) {
      float r = rmax * t / 12.5f, nx = cx + r * cosf(t + a0), ny = cyy + r * sinf(t + a0);
      g->drawLine(px, py, nx, ny, 0); g->drawLine(px + 1, py, nx + 1, ny, 0);
      px = nx; py = ny;
    }
  }
}

static void drawEffects(Arduino_GFX* g, float dt, Mood m) {
  bool cry = m == M_CRY, sad = m == M_SAD || cry, sleepy = m == M_SLEEPY || sleeping, excited = m == M_EXCITED;
  for (int i = 0; i < 2; i++) {                                   // tears
    if (!sad) continue;
    tearY[i] += dt * (cry ? 90.f : 45.f);
    if (tearY[i] > 55) tearY[i] = 0;
    float k = 1.f - tearY[i] / 60.f;
    g->fillCircle(eyeX[i] + (i ? 1 : -1) * eyeW[i] * 0.2f, eyeY[i] + eyeH[i] * 0.45f + tearY[i], 5, col(110 * k, 176 * k, 255 * k));
  }
  float tb = (m == M_SHY) ? 1.f : (m == M_LOVE) ? 0.7f : (m == M_HAPPY || excited) ? 0.3f : 0.f;     // blush
  cblush += (tb - cblush) * fminf(1.f, dt * 6);
  if (cblush > 0.05f) for (int i = 0; i < 2; i++)
    g->fillEllipse(eyeX[i] + (i ? 1 : -1) * 12, eyeY[i] + eyeH[i] * 0.5f + 6, 18, 8, col(255 * cblush * 0.6f, 96 * cblush * 0.6f, 144 * cblush * 0.6f));
  if (excited) for (int i = 0; i < 4; i++) {                      // sparkles
    spark[i] += dt * 1.6f;
    if (spark[i] > 1.f) { spark[i] = 0; sparkX[i] = CX + ((i & 1) ? 1 : -1) * random(45, 135); sparkYp[i] = EYE_CY + random(-65, 35); }
    float s = sinf(3.14159f * spark[i]) * 7.f;
    uint16_t c = col(255, 240, 160);
    g->drawLine(sparkX[i] - s, sparkYp[i], sparkX[i] + s, sparkYp[i], c);
    g->drawLine(sparkX[i], sparkYp[i] - s, sparkX[i], sparkYp[i] + s, c);
  }
  if (sleepy) for (int i = 0; i < 3; i++) {                       // z z z
    zLife[i] += dt * 0.35f;
    if (zLife[i] > 1.f) zLife[i] = 0;
    float k = sinf(3.14159f * zLife[i]);
    g->setTextColor(col(154 * k, 176 * k, 208 * k));
    g->setTextSize(2 + (i & 1));
    g->setCursor(CX + 75 + zLife[i] * 30 + i * 8, EYE_CY - 25 - zLife[i] * 60);
    g->print("z");
  }
}

// ---------- one frame ----------
void update(uint32_t dtMs) {
  Arduino_GFX* g = display::canvas();
  if (testPattern) {                                              // display test: solid colours / grey ramp
    static const uint16_t C[] = {0, 0xF800, 0x07E0, 0x001F, 0xFFFF, 0x0000};
    if (testPattern == 6) for (int x = 0; x < LCD_W; x++) { uint8_t v = x * 255 / (LCD_W - 1); g->drawFastVLine(x, 0, LCD_H, g->color565(v, v, v)); }
    else g->fillScreen(C[testPattern > 5 ? 5 : testPattern]);
    display::flush();
    return;
  }
  float dt = dtMs / 1000.f, k = 1.f - expf(-(float)dtMs / 90.f);

  if (reqFlag) { reqFlag = false; mood = reqMood; holdLeft = reqHold; }
  else if (holdLeft > 0) { holdLeft = holdLeft > dtMs ? holdLeft - dtMs : 0; if (!holdLeft) mood = baseMood; }
  MoodDef t = MOODS[sleeping ? M_SLEEPY : mood];
  if (sleeping) { t.topCover = 0.92f; t.h = 56; t.color = 0x203040; }

  if (millis() > autoLookBlock && !sleeping) {                    // look around on its own
    if (lookIn > dtMs) lookIn -= dtMs;
    else { lookIn = random(1200, 3800); lookTX = random(0, 100) < 30 ? 0 : random(-80, 81) / 100.f; lookTY = random(-30, 31) / 100.f; }
  }
  float fx = t.lookX < 900 ? t.lookX : lookTX, fy = t.lookY < 900 ? t.lookY : lookTY;
  if (sleeping) { fx = 0; fy = 0.3f; }
  lookX = approach(lookX, fx, k * 0.8f);
  lookY = approach(lookY, fy, k * 0.8f);

  cw = approach(cw, t.w, k); ch = approach(ch, t.h, k); cround = approach(cround, t.round, k);
  ctop = approach(ctop, t.topCover, k); cang = approach(cang, t.topAng, k); cbot = approach(cbot, t.botCover, k);
  cy = approach(cy, t.yShift, k);
  cr = approach(cr, (t.color >> 16) & 255, k); cg = approach(cg, (t.color >> 8) & 255, k); cb = approach(cb, t.color & 255, k);
  cspiral = approach(cspiral, t.spiral, k); cheart = approach(cheart, t.heart, k);

  float blink = 0;                                                // blink
  bool canBlink = !(mood == M_DIZZY || mood == M_LOVE) || sleeping;
  if (blinkT > 0) {
    blinkT += dtMs;
    uint32_t dur = (mood == M_SLEEPY || sleeping) ? 320 : 170;
    if (blinkT >= dur) { blinkT = 0; blinkIn = random(2200, 5500); } else blink = sinf(3.14159f * blinkT / (float)dur);
  } else if (canBlink) { if (blinkIn > dtMs) blinkIn -= dtMs; else blinkT = 1; }

  pulse += dt * 6; spin += dt * 420;
  float breathe = 1.f + 0.015f * sinf(pulse * 0.5f);
  float jitter = mood == M_SCARED && !sleeping ? sinf(pulse * 22) * 2.5f : 0;
  float lis = mood == M_LISTENING ? 1.f + 0.05f * sinf(pulse * 1.3f) : 1.f;

  g->fillScreen(0);
  uint16_t c = col(cr, cg, cb);
  for (int i = 0; i < 2; i++) {
    int side = i == 0 ? -1 : 1;
    float w = cw * breathe * lis, h = ch * breathe * lis * (1.f - 0.92f * blink);
    if (lookX * side > 0.1f) w *= 1.f - 0.12f * fabsf(lookX);
    if (mood == M_THINKING && side == 1) { w *= 0.85f; h *= 0.85f; }
    if (h < 4) h = 4;
    float ex = CX + side * EYE_GAP / 2 + lookX * 16 + jitter;
    float ey = EYE_CY + cy + lookY * 12 + (mood == M_DIZZY ? sinf(pulse) * 4 : 0);
    eyeX[i] = ex; eyeY[i] = ey; eyeW[i] = w; eyeH[i] = h;
    drawEye(g, side, ex, ey, w, h, c);
  }
  drawEffects(g, dt, sleeping ? M_SLEEPY : mood);
  display::flush();
}

uint32_t ledColor() { return ((uint32_t)cr << 16) | ((uint32_t)cg << 8) | (uint32_t)cb; }

}  // namespace face
