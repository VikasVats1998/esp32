// EmoPro - multi-mode showcase
// Board: Waveshare ESP32-C6-LCD-1.47 (ST7789 172x320, landscape)
// Press BOOT button to switch modes: EMO face, Flappy EMO game, Matrix rain, Plasma, Starfield, Fireworks
// In the game: press = flap, in the bulb: press = on/off; hold 1s = next mode
// The onboard RGB LED follows what is on screen.

#include <Arduino_GFX_Library.h>
#include <Preferences.h>

// ---- Board pins ----
#define PIN_MOSI 6
#define PIN_SCLK 7
#define PIN_CS   14
#define PIN_DC   15
#define PIN_RST  21
#define PIN_BL   22
#define PIN_LED  8   // WS2812 RGB LED
#define PIN_BTN  9   // BOOT button

#define W 320
#define H 172

Arduino_DataBus *bus = new Arduino_ESP32SPI(PIN_DC, PIN_CS, PIN_SCLK, PIN_MOSI, GFX_NOT_DEFINED);
Arduino_GFX *panel = new Arduino_ST7789(bus, PIN_RST, 1, true, 172, 320, 34, 0, 34, 0);
Arduino_Canvas *gfx = new Arduino_Canvas(W, H, panel);
uint16_t *fb;

#define BLACK 0x0000

static inline uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}
static inline float approach(float c, float t, float k) { return c + (t - c) * k; }

void led(uint8_t r, uint8_t g, uint8_t b) { rgbLedWrite(PIN_LED, r / 4, g / 4, b / 4); } // dimmed

// ============================================================
// Mode 0: EMO face
// ============================================================
namespace Face {
enum Mood { NORMAL, HAPPY, ANGRY, SAD, TIRED, SURPRISED, LOVE, SUSPICIOUS, COUNT };
struct Eye { float x, y, w, h, tx, ty, tw, th; };
Eye L, R;
Mood mood = NORMAL;
uint16_t col = 0x07FF;
uint8_t lr = 0, lg = 255, lb = 255;
float tLookX = 0, tLookY = 0, blink = 1, lid = 0, tLid = 0, bounce = 0;
int blinkPhase = 0;
unsigned long nextBlink, nextLook, nextMood;
const float BW = 70, BH = 80, GAP = 46;

void targets() {
  float lw = BW, lh = BH, rw = BW, rh = BH;
  switch (mood) {
    case SURPRISED:  lw = rw = 82; lh = rh = 96; break;
    case HAPPY:      lh = rh = 70; break;
    case TIRED:      lh = rh = 60; break;
    case SUSPICIOUS: lh = 46; rh = 84; break;
    case LOVE:       lw = rw = lh = rh = 76; break;
    default: break;
  }
  float p = tLookX * 0.12f;
  L.tw = lw * (1 - p); L.th = lh * (1 - p);
  R.tw = rw * (1 + p); R.th = rh * (1 + p);
  float cx = W / 2 + tLookX * 50, cy = H / 2 + tLookY * 30;
  L.tx = cx - GAP / 2 - BW / 2;
  R.tx = cx + GAP / 2 + BW / 2;
  L.ty = R.ty = cy;
  tLid = (mood == ANGRY || mood == SAD || mood == TIRED || mood == HAPPY) ? 1 : 0;
}

void setMood(Mood m) {
  mood = m;
  switch (m) {
    case ANGRY: col = 0xF800; lr = 255; lg = 0;   lb = 0;   break;
    case SAD:   col = 0x3D9F; lr = 40;  lg = 80;  lb = 255; break;
    case LOVE:  col = 0xF8B2; lr = 255; lg = 30;  lb = 120; break;
    case TIRED: col = 0x04BF; lr = 0;   lg = 40;  lb = 120; break;
    case HAPPY: col = 0x07FF; lr = 255; lg = 200; lb = 0;   break;
    default:    col = 0x07FF; lr = 0;   lg = 255; lb = 255; break;
  }
  lid = 0;
  targets();
}

void heart(float cx, float cy, float s) {
  float r = s * 0.28f;
  gfx->fillCircle(cx - r, cy - r * 0.4f, r, col);
  gfx->fillCircle(cx + r, cy - r * 0.4f, r, col);
  gfx->fillTriangle(cx - 2 * r, cy - r * 0.1f, cx + 2 * r, cy - r * 0.1f, cx, cy + s * 0.55f, col);
}

void eye(const Eye &e, bool left) {
  float h = max(4.0f, e.h * blink);
  int x = e.x - e.w / 2, y = e.y - h / 2, w = e.w, hh = h;
  if (mood == LOVE && blink > 0.5f) { heart(e.x, e.y, e.w * (1 + 0.08f * sinf(millis() / 120.0f))); return; }
  int r = max(2, min(w, hh) / 4);
  gfx->fillRoundRect(x, y, w, hh, r, col);
  if (lid < 0.02f || blink < 0.3f) return;
  switch (mood) {
    case ANGRY: { int d = hh * 0.55f * lid;
      if (left) gfx->fillTriangle(x - 1, y - 1, x + w + 1, y - 1, x + w + 1, y + d, BLACK);
      else      gfx->fillTriangle(x - 1, y - 1, x + w + 1, y - 1, x - 1, y + d, BLACK); break; }
    case SAD: { int d = hh * 0.5f * lid;
      if (left) gfx->fillTriangle(x - 1, y - 1, x + w + 1, y - 1, x - 1, y + d, BLACK);
      else      gfx->fillTriangle(x - 1, y - 1, x + w + 1, y - 1, x + w + 1, y + d, BLACK); break; }
    case TIRED: gfx->fillRect(x - 1, y - 1, w + 2, hh * 0.45f * lid, BLACK); break;
    case HAPPY: { int off = hh * 0.55f * lid;
      gfx->fillRoundRect(x - 2, y + hh - off, w + 4, hh, r, BLACK);
      gfx->fillCircle(e.x, y + hh + w * 0.35f - off, w * 0.62f, BLACK); break; }
    default: break;
  }
}

void mouth() {
  int cx = (L.x + R.x) / 2, y = max(L.y + L.h * blink / 2, R.y + R.h * blink / 2) + 14;
  if (y > H - 6) return;
  if (mood == HAPPY) gfx->fillArc(cx, y - 6, 14, 10, 20, 160, col);
  else if (mood == SURPRISED) { gfx->fillCircle(cx, y + 2, 7, col); gfx->fillCircle(cx, y + 2, 3, BLACK); }
  else if (mood == SAD) gfx->fillArc(cx, y + 14, 14, 10, 200, 340, col);
}

void start() {
  L = {W / 2.0f - 58, H / 2.0f, BW, 4};
  R = {W / 2.0f + 58, H / 2.0f, BW, 4};
  setMood(NORMAL);
  unsigned long n = millis();
  nextBlink = n + 1200; nextLook = n + 800; nextMood = n + 5000;
}

void frame() {
  unsigned long now = millis();
  if (blinkPhase == 0 && now > nextBlink) blinkPhase = 1;
  if (blinkPhase == 1) { blink -= 0.25f; if (blink <= 0) { blink = 0; blinkPhase = 2; } }
  else if (blinkPhase == 2) { blink += 0.2f;
    if (blink >= 1) { blink = 1; blinkPhase = 0; nextBlink = now + (random(5) == 0 ? 120 : random(2000, 5000)); } }

  if (now > nextLook) {
    if (random(3) == 0) tLookX = tLookY = 0;
    else { tLookX = random(-100, 101) / 100.0f; tLookY = random(-100, 101) / 100.0f; }
    targets();
    nextLook = now + random(800, 3000);
  }
  if (now > nextMood) {
    Mood m = (Mood)random(COUNT);
    if (m == mood) m = NORMAL;
    setMood(m);
    nextMood = now + (m == NORMAL ? random(5000, 9000) : random(3000, 5500));
  }

  const float k = 0.22f;
  L.x = approach(L.x, L.tx, k); L.y = approach(L.y, L.ty, k); L.w = approach(L.w, L.tw, k); L.h = approach(L.h, L.th, k);
  R.x = approach(R.x, R.tx, k); R.y = approach(R.y, R.ty, k); R.w = approach(R.w, R.tw, k); R.h = approach(R.h, R.th, k);
  lid = approach(lid, tLid, 0.15f);
  if (mood == HAPPY || mood == LOVE) { float b = sinf(now / 150.0f) * 4; L.y += b - bounce; R.y += b - bounce; bounce = b; }
  else bounce = 0;
  if (mood == ANGRY && lid > 0.8f) { float s = random(-2, 3); L.x += s; R.x += s; }

  gfx->fillScreen(BLACK);
  eye(L, true); eye(R, false); mouth();
  if (mood == TIRED && (now / 600) % 2) { gfx->setTextColor(col); gfx->setTextSize(2); gfx->setCursor(R.x + 40, R.y - 60); gfx->print("z"); }

  float pulse = (mood == LOVE) ? 0.5f + 0.5f * sinf(now / 120.0f) : blink;
  led(lr * pulse, lg * pulse, lb * pulse);
}
}

// ============================================================
// Mode 1: Matrix rain
// ============================================================
namespace Matrix {
const int CW = 8, CH = 10, COLS = W / CW, ROWS = H / CH + 1;
char glyph[COLS][ROWS];
float head[COLS], speed[COLS];
int len[COLS];

void start() {
  for (int c = 0; c < COLS; c++) {
    for (int r = 0; r < ROWS; r++) glyph[c][r] = 33 + random(94);
    head[c] = random(-ROWS * 2, ROWS); speed[c] = random(15, 45) / 100.0f; len[c] = random(6, 18);
  }
}

void frame() {
  gfx->fillScreen(BLACK);
  gfx->setTextSize(1);
  for (int c = 0; c < COLS; c++) {
    head[c] += speed[c];
    if (head[c] - len[c] > ROWS) { head[c] = -random(2, 12); speed[c] = random(15, 45) / 100.0f; len[c] = random(6, 18); }
    if (random(8) == 0) glyph[c][random(ROWS)] = 33 + random(94);
    int hd = (int)head[c];
    for (int r = max(0, hd - len[c]); r <= min(ROWS - 1, hd); r++) {
      int d = hd - r;
      uint16_t color = (d == 0) ? rgb(200, 255, 200) : rgb(0, 255 - d * 230 / len[c], 0);
      gfx->drawChar(c * CW, r * CH, glyph[c][r], color, BLACK);
    }
  }
  led(0, 120 + 100 * sinf(millis() / 300.0f), 0);
}
}

// ============================================================
// Mode 2: Plasma
// ============================================================
namespace Plasma {
uint8_t sinT[256];
uint16_t pal[256];

void start() {
  for (int i = 0; i < 256; i++) {
    sinT[i] = 128 + 127 * sinf(i * 2 * PI / 256);
    pal[i] = rgb(128 + 127 * sinf(i * PI / 64), 128 + 127 * sinf(i * PI / 128 + 2), 128 + 127 * sinf(i * PI / 32 + 4));
  }
}

void frame() {
  uint32_t t = millis() / 16;
  uint8_t t1 = t * 2, t2 = t * 3, t3 = t;
  for (int y = 0; y < H; y++) {
    uint8_t sy = sinT[(uint8_t)(y * 3 + t2)];
    uint16_t *row = fb + y * W;
    for (int x = 0; x < W; x++) {
      uint8_t v = (sinT[(uint8_t)(x * 2 + t1)] + sy + sinT[(uint8_t)(x + y + t3)] + sinT[(uint8_t)((x - y) / 2 + t1)]) >> 2;
      row[x] = pal[(uint8_t)(v + t)];
    }
  }
  uint8_t i = t;
  led(128 + 127 * sinf(i * PI / 64), 128 + 127 * sinf(i * PI / 128 + 2), 128 + 127 * sinf(i * PI / 32 + 4));
}
}

// ============================================================
// Mode 3: Starfield (warp speed)
// ============================================================
namespace Stars {
const int N = 220;
float sx[N], sy[N], sz[N], pz[N];

void reset(int i) { sx[i] = random(-1000, 1000); sy[i] = random(-1000, 1000); sz[i] = pz[i] = random(100, 1000); }
void start() { for (int i = 0; i < N; i++) reset(i); }

void frame() {
  gfx->fillScreen(BLACK);
  float spd = 10 + 8 * sinf(millis() / 2000.0f);
  for (int i = 0; i < N; i++) {
    pz[i] = sz[i];
    sz[i] -= spd;
    if (sz[i] < 1) { reset(i); continue; }
    int x = W / 2 + sx[i] / sz[i] * 120, y = H / 2 + sy[i] / sz[i] * 120;
    int px = W / 2 + sx[i] / pz[i] * 120, py = H / 2 + sy[i] / pz[i] * 120;
    if (x < 0 || x >= W || y < 0 || y >= H) { reset(i); continue; }
    uint8_t b = 255 - sz[i] * 230 / 1000;
    uint16_t c = rgb(b, b, min(255, b + 40));
    gfx->drawLine(px, py, x, y, c);
    if (sz[i] < 300) gfx->fillRect(x, y, 2, 2, c);
  }
  uint8_t s = spd * 12;
  led(s / 2, s / 2, s);
}
}

// ============================================================
// Mode 4: Fireworks
// ============================================================
namespace Fireworks {
const int MAXP = 400;
struct P { float x, y, vx, vy; uint16_t life; uint8_t r, g, b; bool rocket; };
P p[MAXP];
unsigned long nextLaunch;
uint8_t flash = 0, fr, fg, fb_;

int freeSlot() { for (int i = 0; i < MAXP; i++) if (!p[i].life) return i; return -1; }

void start() { for (int i = 0; i < MAXP; i++) p[i].life = 0; nextLaunch = 0; }

void explode(float x, float y) {
  uint8_t r = random(80, 256), g = random(80, 256), b = random(80, 256);
  fr = r; fg = g; fb_ = b; flash = 255;
  int n = random(50, 90);
  for (int k = 0; k < n; k++) {
    int i = freeSlot(); if (i < 0) return;
    float a = random(628) / 100.0f, s = random(50, 300) / 100.0f;
    p[i] = {x, y, cosf(a) * s, sinf(a) * s, (uint16_t)random(40, 70), r, g, b, false};
  }
}

void frame() {
  unsigned long now = millis();
  if (now > nextLaunch) {
    int i = freeSlot();
    if (i >= 0) p[i] = {(float)random(40, W - 40), (float)H, random(-60, 60) / 100.0f, -random(450, 600) / 100.0f, 200, 255, 220, 150, true};
    nextLaunch = now + random(300, 1100);
  }
  // fade trails instead of clearing
  for (int i = 0; i < W * H; i++) fb[i] = (fb[i] >> 1) & 0x7BEF;

  for (int i = 0; i < MAXP; i++) {
    P &q = p[i];
    if (!q.life) continue;
    q.x += q.vx; q.y += q.vy; q.vy += q.rocket ? 0.08f : 0.05f;
    if (!q.rocket) { q.vx *= 0.98f; q.vy *= 0.98f; }
    q.life--;
    if (q.rocket && q.vy >= -0.5f) { q.life = 0; explode(q.x, q.y); continue; }
    if (q.x < 0 || q.x >= W - 2 || q.y < 0 || q.y >= H - 2) { if (!q.rocket || q.y < 0) q.life = 0; continue; }
    float f = q.rocket ? 1 : min(1.0f, q.life / 30.0f);
    gfx->fillRect(q.x, q.y, 2, 2, rgb(q.r * f, q.g * f, q.b * f));
  }
  led(fr * flash / 255, fg * flash / 255, fb_ * flash / 255);
  flash = flash > 12 ? flash - 12 : 0;
}
}

// ============================================================
// Mode 5: Flappy EMO game (press = flap, hold 1s = exit)
// ============================================================
namespace Game {
enum State { READY, PLAY, DEAD };
State st;
struct Pipe { float x; int gapY; bool scored; };
const int NP = 4, PW = 32, GAPH = 64, SPACING = 115, GROUND = H - 12;
const int BX = 70, BW = 26, BH = 22;
Pipe pipes[NP];
float by, vy, speed, groundOff;
int score, best;
unsigned long deadAt, happyUntil;
float starX[40], starY[40];
float lr, lg, lb;
Preferences prefs;

void newPipe(Pipe &p, float x) { p.x = x; p.gapY = random(20, GROUND - 20 - GAPH); p.scored = false; }

void reset() {
  by = H / 2; vy = 0; score = 0; speed = 2.0f;
  for (int i = 0; i < NP; i++) newPipe(pipes[i], W + 80 + i * SPACING);
  st = READY;
}

void start() {
  prefs.begin("emo", false);
  best = prefs.getInt("best", 0);
  for (int i = 0; i < 40; i++) { starX[i] = random(W); starY[i] = random(GROUND); }
  lr = lg = lb = 0;
  reset();
}

void flash(float r, float g, float b) { lr = r; lg = g; lb = b; }

void press() {
  if (st == READY) { st = PLAY; vy = -4.6f; }
  else if (st == PLAY) vy = -4.6f;
  else if (millis() - deadAt > 700) reset();
}

void die() {
  st = DEAD; deadAt = millis(); vy = -3;
  if (score > best) { best = score; prefs.putInt("best", best); }
  flash(255, 0, 0);
}

void drawBird() {
  int x0 = BX - BW / 2, y0 = by - BH / 2;
  gfx->fillRoundRect(x0, y0, BW, BH, 7, rgb(225, 228, 240));
  gfx->fillRoundRect(x0 + 3, y0 + 3, BW - 6, BH - 6, 5, BLACK);
  uint16_t c = 0x07FF;
  int ey = by + (vy < -1 ? -2 : (vy > 2 ? 1 : 0));
  int lx = x0 + 8, rx = x0 + 17;
  if (st == DEAD) {
    c = 0xF800;
    for (int ex : {lx, rx}) { gfx->drawLine(ex - 2, by - 3, ex + 2, by + 1, c); gfx->drawLine(ex + 2, by - 3, ex - 2, by + 1, c); }
  } else if (millis() < happyUntil) {
    gfx->fillArc(lx, ey + 2, 4, 2, 180, 360, c);
    gfx->fillArc(rx, ey + 2, 4, 2, 180, 360, c);
  } else {
    int eh = vy < -3 ? 3 : 7;
    gfx->fillRoundRect(lx - 2, ey - eh / 2, 5, eh, 2, c);
    gfx->fillRoundRect(rx - 2, ey - eh / 2, 5, eh, 2, c);
  }
}

void drawPipe(const Pipe &p) {
  int x = p.x, bot = p.gapY + GAPH;
  uint16_t body = rgb(40, 170, 60), cap = rgb(70, 220, 90), hi = rgb(160, 255, 170);
  gfx->fillRect(x, 0, PW, p.gapY, body);
  gfx->fillRect(x, bot, PW, GROUND - bot, body);
  gfx->fillRect(x + 5, 0, 3, p.gapY, hi);
  gfx->fillRect(x + 5, bot, 3, GROUND - bot, hi);
  gfx->fillRect(x - 3, p.gapY - 9, PW + 6, 9, cap);
  gfx->fillRect(x - 3, bot, PW + 6, 9, cap);
}

void centerText(const char *s, int y, int size, uint16_t c) {
  gfx->setTextSize(size);
  gfx->setTextColor(c);
  gfx->setCursor(W / 2 - (int)strlen(s) * 3 * size, y);
  gfx->print(s);
}

void frame() {
  unsigned long now = millis();
  float scroll = (st == PLAY) ? speed : (st == READY ? 1.0f : 0);

  // Physics
  if (st == READY) by = H / 2 + sinf(now / 250.0f) * 6;
  else if (st == PLAY) {
    vy += 0.3f; by += vy;
    if (by < BH / 2) { by = BH / 2; vy = 0; }
    for (int i = 0; i < NP; i++) {
      Pipe &p = pipes[i];
      p.x -= speed;
      if (p.x < -PW - 4) {
        float maxX = 0;
        for (int j = 0; j < NP; j++) maxX = max(maxX, pipes[j].x);
        newPipe(p, maxX + SPACING);
      }
      if (!p.scored && p.x + PW < BX - BW / 2) {
        p.scored = true; score++; speed = min(4.0f, speed + 0.06f);
        happyUntil = now + 350; flash(0, 255, 60);
      }
      if (BX + BW / 2 - 3 > p.x - 3 && BX - BW / 2 + 3 < p.x + PW + 3 &&
          (by - BH / 2 + 3 < p.gapY || by + BH / 2 - 3 > p.gapY + GAPH)) die();
    }
    if (by + BH / 2 >= GROUND) die();
  } else { // DEAD: fall down
    vy += 0.4f; by = min(by + vy, (float)(GROUND - BH / 2));
  }
  groundOff += scroll;

  // Background
  gfx->fillScreen(rgb(8, 10, 30));
  for (int i = 0; i < 40; i++) {
    starX[i] -= scroll * 0.25f;
    if (starX[i] < 0) { starX[i] += W; starY[i] = random(GROUND); }
    gfx->drawPixel(starX[i], starY[i], (i % 3) ? rgb(90, 90, 140) : rgb(200, 200, 255));
  }

  if (st != READY) for (int i = 0; i < NP; i++) if (pipes[i].x < W) drawPipe(pipes[i]);

  // Ground
  gfx->fillRect(0, GROUND, W, H - GROUND, rgb(110, 70, 35));
  gfx->fillRect(0, GROUND, W, 3, rgb(80, 210, 80));
  for (int x = -((int)groundOff % 16); x < W; x += 16) gfx->fillRect(x, GROUND + 6, 8, 2, rgb(80, 50, 25));

  drawBird();

  // HUD
  char buf[24];
  if (st == READY) {
    centerText("FLAPPY EMO", 22, 3, 0xFFE0);
    centerText("press BOOT to flap", 120, 1, 0xFFFF);
    snprintf(buf, sizeof buf, "best: %d", best);
    centerText(buf, 134, 1, 0x07FF);
  } else {
    snprintf(buf, sizeof buf, "%d", score);
    centerText(buf, 9, 3, BLACK);
    centerText(buf, 8, 3, 0xFFFF);
  }
  if (st == DEAD) {
    gfx->fillRoundRect(W / 2 - 80, 40, 160, 82, 10, rgb(20, 20, 50));
    gfx->drawRoundRect(W / 2 - 80, 40, 160, 82, 10, 0x07FF);
    centerText("GAME OVER", 50, 2, 0xF800);
    snprintf(buf, sizeof buf, "score %d  best %d", score, best);
    centerText(buf, 78, 1, 0xFFFF);
    if (score == best && score > 0) centerText("NEW RECORD!", 92, 1, 0xFFE0);
    if (now - deadAt > 700 && (now / 400) % 2) centerText("press to retry", 106, 1, 0x07FF);
  }

  led(lr, lg, lb);
  lr *= 0.9f; lg *= 0.9f; lb *= 0.9f;
}
}

// ============================================================
// Mode 2: Light bulb (press = on/off, hold 1s = exit)
// ============================================================
namespace Bulb {
const int CX = 160, CY = 66, R = 44;
bool on = false;
float bright = 0, heat = 0, rayAngle = 0;
unsigned long switchedAt, lastPress;

static inline uint16_t mix(uint8_t r1, uint8_t g1, uint8_t b1, uint8_t r2, uint8_t g2, uint8_t b2, float t) {
  t = constrain(t, 0.0f, 1.0f);
  return rgb(r1 + (r2 - r1) * t, g1 + (g2 - g1) * t, b1 + (b2 - b1) * t);
}

void start() { on = false; bright = heat = 0; switchedAt = lastPress = millis(); }

void toggle() { on = !on; switchedAt = lastPress = millis(); }

void filament(uint16_t c) {
  gfx->drawLine(151, 112, 152, 74, c); gfx->drawLine(152, 112, 153, 74, c);
  gfx->drawLine(169, 112, 168, 74, c); gfx->drawLine(168, 112, 167, 74, c);
  for (int i = 0; i < 8; i++) { // coil
    int x1 = 152 + i * 2, y1 = (i % 2) ? 70 : 76;
    gfx->drawLine(x1, y1, x1 + 2, (i % 2) ? 76 : 70, c);
  }
  gfx->drawLine(152, 74, 152, 76, c); gfx->drawLine(168, 74, 168, 76, c);
}

void frame() {
  unsigned long now = millis();
  if (now - lastPress > 6000) toggle(); // demo: auto-toggle when idle

  // Brightness: flicker while warming up, quick drop when off
  if (on) {
    unsigned long dt = now - switchedAt;
    if (dt < 450) bright = (random(3) == 0) ? random(20, 100) / 100.0f : bright * 0.6f;
    else bright = approach(bright, 1.0f, 0.15f);
  } else bright = approach(bright, 0, 0.3f);
  heat = approach(heat, bright, on ? 0.2f : 0.035f); // filament cools slowly (afterglow)
  float b = bright * (0.97f + 0.03f * sinf(now / 40.0f));

  // Background warms up
  gfx->fillScreen(mix(6, 6, 12, 60, 40, 8, b));

  // Glow halo
  for (int r = 130; r > R; r -= 7) {
    float f = b * powf(1.0f - (float)(r - R) / (130 - R), 2.2f);
    gfx->fillCircle(CX, CY, r, mix(6 + 54 * b, 6 + 34 * b, 12 - 4 * b, 255, 210, 90, f));
  }

  // Rotating rays
  if (b > 0.05f) {
    rayAngle += 0.01f;
    uint16_t rc = mix(60, 40, 8, 255, 230, 120, b * 0.8f);
    for (int i = 0; i < 12; i++) {
      float a = rayAngle + i * PI / 6, w = 0.06f;
      float r1 = R + 12, r2 = R + 30 + 10 * sinf(now / 200.0f + i);
      gfx->fillTriangle(CX + cosf(a - w) * r1, CY + sinf(a - w) * r1, CX + cosf(a + w) * r1, CY + sinf(a + w) * r1,
                        CX + cosf(a) * r2, CY + sinf(a) * r2, rc);
    }
  }

  // Glass bulb + neck
  uint16_t glass = mix(38, 40, 52, 255, 248, 200, b);
  gfx->fillCircle(CX, CY, R, glass);
  gfx->fillTriangle(CX - 30, CY + 32, CX + 30, CY + 32, CX - 18, 118, glass);
  gfx->fillTriangle(CX + 30, CY + 32, CX + 18, 118, CX - 18, 118, glass);
  gfx->fillRect(CX - 18, 112, 36, 7, glass);
  // Highlight reflection
  gfx->fillCircle(CX - 20, CY - 18, 7, mix(70, 72, 90, 255, 255, 240, b));
  gfx->fillCircle(CX - 26, CY - 6, 3, mix(70, 72, 90, 255, 255, 240, b));

  // Filament: grey -> red -> orange -> white with heat
  uint16_t fc = heat < 0.5f ? mix(90, 90, 95, 255, 80, 0, heat * 2) : mix(255, 80, 0, 255, 255, 220, (heat - 0.5f) * 2);
  filament(fc);

  // Screw base
  for (int i = 0; i < 5; i++) {
    uint16_t c = (i % 2) ? rgb(120, 120, 128) : rgb(185, 185, 195);
    gfx->fillRoundRect(CX - 19 + i, 119 + i * 6, 38 - i * 2, 6, 2, c);
  }
  gfx->fillRoundRect(CX - 8, 149, 16, 7, 3, rgb(40, 40, 45));

  // Label
  gfx->setTextSize(2);
  gfx->setTextColor(on ? 0xFFE0 : 0x8410);
  gfx->setCursor(262, 148);
  gfx->print(on ? "ON" : "OFF");
  gfx->setTextSize(1);
  gfx->setTextColor(0x8410);
  gfx->setCursor(8, 158);
  gfx->print("press BOOT");

  // Real light: board LED + screen backlight follow the bulb
  led(255 * b, 170 * b, 40 * b);
  analogWrite(PIN_BL, 110 + 145 * b);
}
}

// ============================================================
// Mode manager
// ============================================================
const char *names[] = {"EMO", "GAME", "BULB", "MATRIX", "PLASMA", "WARP", "FIREWORKS"};
const int MODES = 7;
const int GAME_MODE = 1, BULB_MODE = 2;
int mode = 0;
unsigned long labelUntil = 0;
bool lastBtn = HIGH, holdHandled = true;
unsigned long lastFrame = 0, btnTime = 0;

void startMode(int m) {
  mode = m;
  gfx->fillScreen(BLACK);
  analogWrite(PIN_BL, 255);
  switch (m) {
    case 0: Face::start(); break;
    case 1: Game::start(); break;
    case 2: Bulb::start(); break;
    case 3: Matrix::start(); break;
    case 4: Plasma::start(); break;
    case 5: Stars::start(); break;
    case 6: Fireworks::start(); break;
  }
  labelUntil = millis() + 1200;
  Serial.printf("Mode: %s\n", names[m]);
}

void setup() {
  Serial.begin(115200);
  analogWrite(PIN_BL, 255);
  pinMode(PIN_BTN, INPUT_PULLUP);
  if (!gfx->begin(80000000)) Serial.println("Display init failed");
  fb = gfx->getFramebuffer();
  randomSeed(esp_random());
  startMode(0);
}

void loop() {
  // In the game a press flaps and a 1s hold exits; elsewhere a press switches mode
  bool b = digitalRead(PIN_BTN);
  unsigned long now = millis();
  if (b == LOW && lastBtn == HIGH && now - btnTime > 60) {
    btnTime = now;
    if (mode == GAME_MODE) { Game::press(); holdHandled = false; }
    else if (mode == BULB_MODE) { Bulb::toggle(); holdHandled = false; }
    else { startMode((mode + 1) % MODES); holdHandled = true; }
  }
  if (b == LOW && !holdHandled && now - btnTime > 1000) { holdHandled = true; startMode((mode + 1) % MODES); }
  lastBtn = b;

  if (now - lastFrame < 16) return; // ~60 fps cap
  lastFrame = now;

  switch (mode) {
    case 0: Face::frame(); break;
    case 1: Game::frame(); break;
    case 2: Bulb::frame(); break;
    case 3: Matrix::frame(); break;
    case 4: Plasma::frame(); break;
    case 5: Stars::frame(); break;
    case 6: Fireworks::frame(); break;
  }

  if (millis() < labelUntil) {
    gfx->setTextSize(2);
    int w = strlen(names[mode]) * 12;
    gfx->fillRoundRect(W / 2 - w / 2 - 8, 6, w + 16, 24, 6, 0x2104);
    gfx->setTextColor(0xFFFF);
    gfx->setCursor(W / 2 - w / 2, 11);
    gfx->print(names[mode]);
  }
  gfx->flush();
}
