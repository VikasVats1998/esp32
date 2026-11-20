// EMO-style animated robot face
// Board: Waveshare ESP32-C6-LCD-1.47 (ST7789, 172x320, used in landscape 320x172)

#include <Arduino_GFX_Library.h>

// ---- Board pins ----
#define PIN_MOSI 6
#define PIN_SCLK 7
#define PIN_CS   14
#define PIN_DC   15
#define PIN_RST  21
#define PIN_BL   22

#define SCREEN_W 320
#define SCREEN_H 172

Arduino_DataBus *bus = new Arduino_ESP32SPI(PIN_DC, PIN_CS, PIN_SCLK, PIN_MOSI, GFX_NOT_DEFINED);
// rotation 1 = landscape; panel is 172x320 with a 34px column offset
Arduino_GFX *panel = new Arduino_ST7789(bus, PIN_RST, 1, true, 172, 320, 34, 0, 34, 0);
Arduino_Canvas *gfx = new Arduino_Canvas(SCREEN_W, SCREEN_H, panel);

#define BLACK 0x0000
uint16_t eyeColor = 0x07FF; // cyan

// ---- Moods ----
enum Mood { NORMAL, HAPPY, ANGRY, SAD, TIRED, SURPRISED, LOVE, SUSPICIOUS, MOOD_COUNT };

struct Eye {
  float x, y, w, h;          // current (center, size)
  float tx, ty, tw, th;      // target
};

Eye L, R;
Mood mood = NORMAL;
float lookX = 0, lookY = 0;          // current gaze
float tLookX = 0, tLookY = 0;        // target gaze
float blink = 1.0f;                  // 1 = open, 0 = closed
float lid = 0;                       // eyelid overlay amount (0..1) for current mood
float tLid = 0;

unsigned long nextBlink = 0, nextLook = 0, nextMood = 0;
int blinkPhase = 0;                  // 0 none, 1 closing, 2 opening
float bounce = 0;                    // for happy bounce
unsigned long lastFrame = 0;

const float BASE_W = 70, BASE_H = 80, GAP = 46;

static inline float approach(float cur, float target, float k) { return cur + (target - cur) * k; }

void setTargets() {
  float w = BASE_W, h = BASE_H;
  float lw = w, lh = h, rw = w, rh = h;
  switch (mood) {
    case SURPRISED:  lw = rw = 82; lh = rh = 96; break;
    case HAPPY:      lh = rh = 70; break;
    case TIRED:      lh = rh = 60; break;
    case SUSPICIOUS: lh = 46; rh = 84; break;
    case LOVE:       lw = rw = 76; lh = rh = 76; break;
    default: break;
  }
  // Perspective: the eye on the side we look toward gets slightly bigger
  float persp = tLookX * 0.12f;
  L.tw = lw * (1 - persp); L.th = lh * (1 - persp);
  R.tw = rw * (1 + persp); R.th = rh * (1 + persp);

  float cx = SCREEN_W / 2 + tLookX * 50;
  float cy = SCREEN_H / 2 + tLookY * 30;
  L.tx = cx - GAP / 2 - BASE_W / 2;
  R.tx = cx + GAP / 2 + BASE_W / 2;
  L.ty = R.ty = cy;

  tLid = (mood == ANGRY || mood == SAD || mood == TIRED || mood == HAPPY) ? 1 : 0;
}

void drawHeart(float cx, float cy, float s) {
  float r = s * 0.28f;
  gfx->fillCircle(cx - r, cy - r * 0.4f, r, eyeColor);
  gfx->fillCircle(cx + r, cy - r * 0.4f, r, eyeColor);
  gfx->fillTriangle(cx - 2 * r, cy - r * 0.1f, cx + 2 * r, cy - r * 0.1f, cx, cy + s * 0.55f, eyeColor);
}

void drawEye(const Eye &e, bool isLeft) {
  float h = max(4.0f, e.h * blink);
  int x = e.x - e.w / 2, y = e.y - h / 2;
  int w = e.w, hh = h;

  if (mood == LOVE && blink > 0.5f) {
    drawHeart(e.x, e.y, e.w * (1 + 0.08f * sinf(millis() / 120.0f)));
    return;
  }

  int r = min(w, hh) / 4;
  if (r < 2) r = 2;
  gfx->fillRoundRect(x, y, w, hh, r, eyeColor);

  if (lid < 0.02f || blink < 0.3f) return;

  // Mood eyelids drawn as black shapes over the eye
  float a = lid;
  switch (mood) {
    case ANGRY: { // inner corners pushed down
      int d = hh * 0.55f * a;
      if (isLeft) gfx->fillTriangle(x - 1, y - 1, x + w + 1, y - 1, x + w + 1, y + d, BLACK);
      else        gfx->fillTriangle(x - 1, y - 1, x + w + 1, y - 1, x - 1, y + d, BLACK);
      break;
    }
    case SAD: { // outer corners pushed down
      int d = hh * 0.5f * a;
      if (isLeft) gfx->fillTriangle(x - 1, y - 1, x + w + 1, y - 1, x - 1, y + d, BLACK);
      else        gfx->fillTriangle(x - 1, y - 1, x + w + 1, y - 1, x + w + 1, y + d, BLACK);
      break;
    }
    case TIRED: { // flat heavy top lid
      int d = hh * 0.45f * a;
      gfx->fillRect(x - 1, y - 1, w + 2, d, BLACK);
      break;
    }
    case HAPPY: { // bottom cut by a big circle -> ^ ^ shape
      int off = hh * 0.55f * a;
      gfx->fillRoundRect(x - 2, y + hh - off, w + 4, hh, r, BLACK);
      gfx->fillCircle(e.x, y + hh + w * 0.35f - off, w * 0.62f, BLACK);
      break;
    }
    default: break;
  }
}

void drawMouth() {
  int cx = (L.x + R.x) / 2, y = max(L.y + L.h * blink / 2, R.y + R.h * blink / 2) + 14;
  if (y > SCREEN_H - 6) return;
  switch (mood) {
    case HAPPY:
      gfx->fillArc(cx, y - 6, 14, 10, 20, 160, eyeColor);
      break;
    case SURPRISED:
      gfx->fillCircle(cx, y + 2, 7, eyeColor);
      gfx->fillCircle(cx, y + 2, 3, BLACK);
      break;
    case SAD:
      gfx->fillArc(cx, y + 14, 14, 10, 200, 340, eyeColor);
      break;
    default: break;
  }
}

void render() {
  gfx->fillScreen(BLACK);
  drawEye(L, true);
  drawEye(R, false);
  drawMouth();
  if (mood == TIRED && (millis() / 600) % 2) { // little "z" when sleepy
    gfx->setTextColor(eyeColor);
    gfx->setTextSize(2);
    gfx->setCursor(R.x + 40, R.y - 60);
    gfx->print("z");
  }
  gfx->flush();
}

void setMood(Mood m) {
  mood = m;
  switch (m) {
    case HAPPY:      eyeColor = 0x07FF; break;
    case ANGRY:      eyeColor = 0xF800; break; // red
    case SAD:        eyeColor = 0x3D9F; break; // soft blue
    case LOVE:       eyeColor = 0xF8B2; break; // pink
    case TIRED:      eyeColor = 0x04BF; break;
    default:         eyeColor = 0x07FF; break;
  }
  lid = 0;
  setTargets();
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_BL, OUTPUT);
  digitalWrite(PIN_BL, HIGH);

  if (!gfx->begin(80000000)) Serial.println("Display init failed");
  gfx->fillScreen(BLACK);

  randomSeed(esp_random());
  L = {SCREEN_W / 2.0f - 58, SCREEN_H / 2.0f, BASE_W, 4};
  R = {SCREEN_W / 2.0f + 58, SCREEN_H / 2.0f, BASE_W, 4};
  setMood(NORMAL);
  nextBlink = millis() + 1500;
  nextLook = millis() + 1000;
  nextMood = millis() + 6000;
}

void loop() {
  unsigned long now = millis();
  if (now - lastFrame < 16) return; // ~60 fps cap
  lastFrame = now;

  // --- Blinking ---
  if (blinkPhase == 0 && now > nextBlink) blinkPhase = 1;
  if (blinkPhase == 1) { blink -= 0.25f; if (blink <= 0) { blink = 0; blinkPhase = 2; } }
  else if (blinkPhase == 2) {
    blink += 0.2f;
    if (blink >= 1) {
      blink = 1; blinkPhase = 0;
      // sometimes double-blink
      nextBlink = now + (random(5) == 0 ? 120 : random(2000, 5000));
    }
  }

  // --- Gaze ---
  if (now > nextLook) {
    if (random(3) == 0) { tLookX = 0; tLookY = 0; }
    else { tLookX = random(-100, 101) / 100.0f; tLookY = random(-100, 101) / 100.0f; }
    setTargets();
    nextLook = now + random(800, 3000);
  }

  // --- Mood cycling ---
  if (now > nextMood) {
    Mood m = (Mood)random(MOOD_COUNT);
    if (m == mood) m = NORMAL;
    setMood(m);
    Serial.printf("Mood: %d\n", m);
    nextMood = now + (m == NORMAL ? random(5000, 9000) : random(3000, 5500));
  }

  // --- Easing ---
  const float k = 0.22f;
  L.x = approach(L.x, L.tx, k); L.y = approach(L.y, L.ty, k);
  L.w = approach(L.w, L.tw, k); L.h = approach(L.h, L.th, k);
  R.x = approach(R.x, R.tx, k); R.y = approach(R.y, R.ty, k);
  R.w = approach(R.w, R.tw, k); R.h = approach(R.h, R.th, k);
  lid = approach(lid, tLid, 0.15f);

  // Happy / love: gentle bounce
  if (mood == HAPPY || mood == LOVE) {
    float b = sinf(now / 150.0f) * 4;
    L.y += b - bounce; R.y += b - bounce; bounce = b;
  } else bounce = 0;

  // Angry: little shake
  if (mood == ANGRY && lid > 0.8f) {
    float s = random(-2, 3);
    L.x += s; R.x += s;
  }

  render();
}
