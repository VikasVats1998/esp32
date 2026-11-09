// EmoBot - EMO-style walking desk robot
// Board: Waveshare ESP32-C6-LCD-1.47 (ST7789 172x320, landscape)
// Hardware: 4 servos (2 hips + 2 feet), HC-SR04 ultrasonic, 2x TTP223 touch (head + back)
//
// Behaviour:
//   Touch head        -> happy wiggle      | hold head 3s -> go to sleep
//   Touch back        -> giggles (love)    | both at once -> dance
//   Hand close (<12cm)-> surprised, steps back
//   Hand near (<35cm) -> happy "hello" bounce
//   BOOT button       -> toggle PET mode / EXPLORE mode (walks around, avoids obstacles)
//   Ignored ~1 min    -> gets sleepy, ~2 min -> sleeps (any touch wakes it)
//   Hold BOOT while powering on -> servo calibration (all servos at 90 deg)

#include <Arduino_GFX_Library.h>

// ---- Display pins (fixed on this board) ----
#define PIN_MOSI 6
#define PIN_SCLK 7
#define PIN_CS   14
#define PIN_DC   15
#define PIN_RST  21
#define PIN_BL   22
#define PIN_LED  8   // onboard WS2812
#define PIN_BTN  9   // BOOT button

// ---- Robot pins (change to match your wiring) ----
#define PIN_SERVO_LH 0   // left hip
#define PIN_SERVO_RH 1   // right hip
#define PIN_SERVO_LF 2   // left foot
#define PIN_SERVO_RF 3   // right foot
#define PIN_TRIG     18
#define PIN_ECHO     19  // HC-SR04 ECHO is 5V -> use a 1k/2k voltage divider!
#define PIN_TOUCH_HEAD 20
#define PIN_TOUCH_BACK 23

// ---- Servo calibration ----
// trim: degrees added so the robot stands straight at 90
// reverse: flip a servo if it moves the wrong way (depends on how it is mounted)
int  trim[4]    = {0, 0, 0, 0};
bool reverse[4] = {false, false, false, false};
const int servoPin[4] = {PIN_SERVO_LH, PIN_SERVO_RH, PIN_SERVO_LF, PIN_SERVO_RF};

#define W 320
#define H 172
#define BLACK 0x0000

Arduino_DataBus *bus = new Arduino_ESP32SPI(PIN_DC, PIN_CS, PIN_SCLK, PIN_MOSI, GFX_NOT_DEFINED);
Arduino_GFX *panel = new Arduino_ST7789(bus, PIN_RST, 1, true, 172, 320, 34, 0, 34, 0);
Arduino_Canvas *gfx = new Arduino_Canvas(W, H, panel);

static inline float approach(float c, float t, float k) { return c + (t - c) * k; }
void led(uint8_t r, uint8_t g, uint8_t b) { rgbLedWrite(PIN_LED, r / 4, g / 4, b / 4); }
const float DEG = PI / 180;

// ============================================================
// Servos
// ============================================================
float servoPos[4] = {90, 90, 90, 90};
bool servosRelaxed = false;

void servoWrite(int i, float deg) {
  deg = constrain(deg, 0, 180);
  servoPos[i] = deg;
  float d = reverse[i] ? 180 - deg : deg;
  d = constrain(d + trim[i], 0, 180);
  uint32_t us = 500 + d * 2000 / 180;              // 500..2500us pulse
  ledcWrite(servoPin[i], us * 16384 / 20000);      // 50Hz, 14-bit
  servosRelaxed = false;
}

void servosRelax() { // stop pulses: quiet, saves power while sleeping
  for (int i = 0; i < 4; i++) ledcWrite(servoPin[i], 0);
  servosRelaxed = true;
}

// ============================================================
// Motion engine (Otto-style oscillators, non-blocking)
// ============================================================
namespace Motion {
enum Type { WALK, TURN, UPDOWN, SWING, TIPTOE, JITTER, MOONWALK, HOME };
struct Move { Type type; int cycles, T, dir, h; };
const int QN = 16;
Move queue[QN];
int qHead = 0, qLen = 0;
Move cur;
bool active = false;
unsigned long t0, endTime;
float A[4], O[4], P[4], from[4];

void push(Type type, int cycles = 1, int T = 1000, int dir = 1, int h = 20) {
  if (qLen >= QN) return;
  queue[(qHead + qLen) % QN] = {type, cycles, T, dir, h};
  qLen++;
}
void clear() { qLen = 0; active = false; }
bool idle() { return qLen == 0 && (!active || cur.type == HOME); }

void set4(float *a, float a0, float a1, float a2, float a3) { a[0] = a0; a[1] = a1; a[2] = a2; a[3] = a3; }

void prepare(const Move &m) {
  float h = m.h;
  set4(A, 0, 0, 0, 0); set4(O, 0, 0, 0, 0); set4(P, 0, 0, 0, 0);
  switch (m.type) {
    case WALK:     set4(A, 30, 30, 20, 20); set4(O, 0, 0, 4, -4); set4(P, 0, 0, -90 * m.dir * DEG, -90 * m.dir * DEG); break;
    case TURN:     set4(A, m.dir > 0 ? 30 : 10, m.dir > 0 ? 10 : 30, 20, 20); set4(O, 0, 0, 4, -4); set4(P, 0, 0, -90 * DEG, -90 * DEG); break;
    case UPDOWN:   set4(A, 0, 0, h, h); set4(O, 0, 0, h, -h); set4(P, 0, 0, -90 * DEG, 90 * DEG); break;
    case SWING:    set4(A, 0, 0, h, h); set4(O, 0, 0, h / 2, -h / 2); break;
    case TIPTOE:   set4(A, 0, 0, h, h); set4(O, 0, 0, h, -h); break;
    case JITTER:   set4(A, h, h, 0, 0); set4(P, -90 * DEG, 90 * DEG, 0, 0); break;
    case MOONWALK: { float phi = -m.dir * 90 * DEG;
      set4(A, 0, 0, h, h); set4(O, 0, 0, h / 2 + 2, -h / 2 - 2); set4(P, 0, 0, phi, -60 * m.dir * DEG + phi); break; }
    case HOME: break;
  }
  for (int i = 0; i < 4; i++) from[i] = servoPos[i];
}

void update() {
  unsigned long now = millis();
  if (active && cur.type == HOME && qLen) active = false;   // new move interrupts homing
  if (!active) {
    if (qLen) { cur = queue[qHead]; qHead = (qHead + 1) % QN; qLen--; }
    else if (!servosRelaxed && now - endTime > 250 &&
             (servoPos[0] != 90 || servoPos[1] != 90 || servoPos[2] != 90 || servoPos[3] != 90)) cur = {HOME, 1, 400, 1, 0};
    else return;
    active = true; t0 = now; prepare(cur);
  }
  float el = now - t0;
  if (cur.type == HOME) {
    float k = min(1.0f, el / cur.T);
    for (int i = 0; i < 4; i++) servoWrite(i, from[i] + (90 - from[i]) * k);
    if (k >= 1) { active = false; endTime = now; }
    return;
  }
  if (el >= (float)cur.T * cur.cycles) { active = false; endTime = now; return; }
  float ph = 2 * PI * el / cur.T;
  float blend = min(1.0f, el / 150.0f);  // ease in from previous pose
  for (int i = 0; i < 4; i++) {
    float target = 90 + O[i] + A[i] * sinf(ph + P[i]);
    servoWrite(i, from[i] + (target - from[i]) * blend);
  }
}
}

// ============================================================
// Face (EMO eyes)
// ============================================================
namespace Face {
enum Mood { NORMAL, HAPPY, ANGRY, SAD, TIRED, SURPRISED, LOVE, SUSPICIOUS };
struct Eye { float x, y, w, h, tx, ty, tw, th; };
Eye L, R;
Mood mood = NORMAL;
uint16_t col = 0x07FF;
uint8_t lr = 0, lg = 255, lb = 255;
float tLookX = 0, tLookY = 0, blink = 1, lid = 0, tLid = 0, bounce = 0;
int blinkPhase = 0;
unsigned long nextBlink, nextLook, holdUntil = 0, lookHoldUntil = 0;
bool sleeping = false;
const float BW = 70, BH = 80, GAP = 46;

void targets() {
  float lw = BW, lh = BH, rw = BW, rh = BH;
  switch (mood) {
    case SURPRISED:  lw = rw = 82; lh = rh = 96; break;
    case HAPPY:      lh = rh = 70; break;
    case TIRED:      lh = rh = sleeping ? 30 : 60; break;
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

// Show a mood for ms milliseconds (0 = until changed), then back to NORMAL
void show(Mood m, unsigned long ms) { setMood(m); holdUntil = ms ? millis() + ms : 0xFFFFFFFF; }
void look(float x, float y, unsigned long ms) { tLookX = x; tLookY = y; lookHoldUntil = millis() + ms; targets(); }

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

void begin() {
  L = {W / 2.0f - 58, H / 2.0f, BW, 4};
  R = {W / 2.0f + 58, H / 2.0f, BW, 4};
  setMood(NORMAL);
  nextBlink = millis() + 1200; nextLook = millis() + 800;
}

void frame() {
  unsigned long now = millis();
  if (now > holdUntil) { holdUntil = 0xFFFFFFFF; setMood(NORMAL); }

  if (!sleeping) {
    if (blinkPhase == 0 && now > nextBlink) blinkPhase = 1;
    if (blinkPhase == 1) { blink -= 0.25f; if (blink <= 0) { blink = 0; blinkPhase = 2; } }
    else if (blinkPhase == 2) { blink += 0.2f;
      if (blink >= 1) { blink = 1; blinkPhase = 0; nextBlink = now + (random(5) == 0 ? 120 : random(2000, 5000)); } }
    if (now > nextLook && now > lookHoldUntil) {
      if (random(3) == 0) tLookX = tLookY = 0;
      else { tLookX = random(-100, 101) / 100.0f; tLookY = random(-100, 101) / 100.0f; }
      targets();
      nextLook = now + random(800, 3000);
    }
  } else { blink = 1; tLookX = 0; tLookY = 0.6f; targets(); }

  const float k = 0.22f;
  L.x = approach(L.x, L.tx, k); L.y = approach(L.y, L.ty, k); L.w = approach(L.w, L.tw, k); L.h = approach(L.h, L.th, k);
  R.x = approach(R.x, R.tx, k); R.y = approach(R.y, R.ty, k); R.w = approach(R.w, R.tw, k); R.h = approach(R.h, R.th, k);
  lid = approach(lid, tLid, 0.15f);
  if (mood == HAPPY || mood == LOVE) { float b = sinf(now / 150.0f) * 4; L.y += b - bounce; R.y += b - bounce; bounce = b; }
  else bounce = 0;
  if (mood == ANGRY && lid > 0.8f) { float s = random(-2, 3); L.x += s; R.x += s; }

  gfx->fillScreen(BLACK);
  eye(L, true); eye(R, false); mouth();
  if (mood == TIRED && (now / 600) % 2) {
    gfx->setTextColor(col); gfx->setTextSize(sleeping ? 3 : 2);
    gfx->setCursor(R.x + 40, R.y - 60); gfx->print(sleeping ? "Z" : "z");
  }

  if (sleeping) { float br = 0.5f + 0.5f * sinf(now / 900.0f); led(0, 20 * br, 120 * br); }
  else { float p = (mood == LOVE) ? 0.5f + 0.5f * sinf(now / 120.0f) : blink; led(lr * p, lg * p, lb * p); }
}
}

// ============================================================
// Sensors
// ============================================================
float distCm = 999;

void readDistance() {
  digitalWrite(PIN_TRIG, LOW); delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  unsigned long us = pulseIn(PIN_ECHO, HIGH, 15000);  // ~2.5m max
  float d = us ? us / 58.0f : 999;
  if (d >= 999) distCm = 999;
  else distCm = (distCm >= 999) ? d : distCm * 0.4f + d * 0.6f;  // smooth
}

struct Touch {
  int pin; bool state = false; unsigned long since = 0; bool longFired = false;
  bool pressed = false;   // true for one loop on touch
  void update() {
    bool s = digitalRead(pin);
    pressed = false;
    if (s != state && millis() - since > 40) {
      state = s; since = millis(); longFired = false;
      if (s) pressed = true;
    }
  }
  unsigned long held() { return state ? millis() - since : 0; }
};
Touch head{PIN_TOUCH_HEAD}, back{PIN_TOUCH_BACK};

// ============================================================
// Behaviour
// ============================================================
enum BotMode { PET, EXPLORE, SLEEP };
BotMode botMode = PET;
unsigned long lastInteract = 0, nextIdleAct = 0, nextReact = 0, labelUntil = 0, lastDist = 0, lastFrame = 0;
bool lastBtn = HIGH, wasNear = false, wasClose = false;
const char *label = "";

void showLabel(const char *s) { label = s; labelUntil = millis() + 1500; }
void interact() { lastInteract = millis(); }

void goSleep() {
  botMode = SLEEP;
  Motion::clear();
  Motion::push(Motion::HOME);
  Face::sleeping = true;
  Face::show(Face::TIRED, 0);
  showLabel("SLEEP");
  Serial.println("sleep");
}

void wake() {
  Face::sleeping = false;
  botMode = PET;
  Face::show(Face::SURPRISED, 1500);
  Motion::clear();
  Motion::push(Motion::UPDOWN, 2, 600, 1, 20);
  interact();
  showLabel("HI!");
  Serial.println("wake");
}

void behaviour() {
  unsigned long now = millis();

  // BOOT: toggle pet / explore
  bool b = digitalRead(PIN_BTN);
  if (b == LOW && lastBtn == HIGH) {
    if (botMode == SLEEP) wake();
    else {
      botMode = (botMode == PET) ? EXPLORE : PET;
      Motion::clear();
      Face::show(botMode == EXPLORE ? Face::SUSPICIOUS : Face::HAPPY, 1200);
      showLabel(botMode == EXPLORE ? "EXPLORE" : "PET");
      interact();
    }
  }
  lastBtn = b;

  head.update(); back.update();

  if (botMode == SLEEP) {
    if (head.pressed || back.pressed || distCm < 8) wake();
    else if (now - Motion::endTime > 1500 && Motion::idle() && !servosRelaxed) servosRelax();
    return;
  }

  // Touch reactions
  if (head.state && back.state && (head.pressed || back.pressed)) {
    Motion::clear();
    Face::show(Face::LOVE, 6000);
    Motion::push(Motion::MOONWALK, 3, 900, 1, 25);
    Motion::push(Motion::MOONWALK, 3, 900, -1, 25);
    Motion::push(Motion::UPDOWN, 2, 600, 1, 20);
    Motion::push(Motion::SWING, 2, 700, 1, 25);
    showLabel("DANCE!");
    interact();
  } else if (head.pressed) {
    Motion::clear();
    Face::show(Face::HAPPY, 2500);
    Motion::push(Motion::SWING, 3, 600, 1, 15);
    interact();
  } else if (back.pressed) {
    Motion::clear();
    Face::show(Face::LOVE, 3000);
    Motion::push(Motion::JITTER, 4, 350, 1, 15);
    Motion::push(Motion::TIPTOE, 2, 600, 1, 15);
    interact();
  }
  if (head.state && !back.state && head.held() > 3000 && !head.longFired) { head.longFired = true; goSleep(); return; }

  if (botMode == EXPLORE) {
    interact(); // exploring never gets bored
    if (Motion::idle()) {
      if (distCm < 22) {
        int dir = random(2) ? 1 : -1;
        Face::show(Face::SURPRISED, 1200);
        Face::look(dir * 0.9f, 0, 2500);
        Motion::push(Motion::WALK, 2, 900, -1);
        Motion::push(Motion::TURN, 3, 1000, dir);
      } else {
        Face::look(0, 0.2f, 1200);
        Motion::push(Motion::WALK, 1, 1000, 1);
      }
    }
    return;
  }

  // PET mode: react to hands in front
  bool close = distCm < 12, near = distCm < 35;
  if (now > nextReact) {
    if (close && !wasClose) {
      Motion::clear();
      Face::show(Face::SURPRISED, 1500);
      Motion::push(Motion::WALK, 2, 900, -1);
      nextReact = now + 3000; interact();
    } else if (near && !wasNear && Motion::idle()) {
      Face::show(Face::HAPPY, 1500);
      Motion::push(Motion::UPDOWN, 1, 700, 1, 15);
      nextReact = now + 3000; interact();
    }
  }
  if (near) Face::look(0, 0, 300); // stare at whoever is in front
  wasClose = close; wasNear = near;

  // Boredom
  unsigned long bored = now - lastInteract;
  if (bored > 120000) { goSleep(); return; }
  if (bored > 60000 && Face::mood != Face::TIRED) Face::show(Face::TIRED, 0);

  // Random idle gestures
  if (now > nextIdleAct && Motion::idle() && bored < 60000) {
    switch (random(4)) {
      case 0: Motion::push(Motion::SWING, 2, 900, 1, 12); break;
      case 1: Motion::push(Motion::TIPTOE, 1, 800, 1, 12); break;
      case 2: Motion::push(Motion::JITTER, 2, 500, 1, 10); Face::show(Face::SUSPICIOUS, 1500); break;
      case 3: Face::show((Face::Mood)random(1, 8), 2000); break;
    }
    nextIdleAct = now + random(8000, 20000);
  }
}

// ============================================================
// Setup / loop
// ============================================================
void calibration() {
  for (int i = 0; i < 4; i++) servoWrite(i, 90);
  gfx->fillScreen(BLACK);
  gfx->setTextColor(0xFFE0); gfx->setTextSize(2);
  gfx->setCursor(20, 40);  gfx->print("CALIBRATION");
  gfx->setTextColor(0xFFFF); gfx->setTextSize(1);
  gfx->setCursor(20, 75);  gfx->print("All servos are at 90 degrees.");
  gfx->setCursor(20, 90);  gfx->print("Attach horns so legs are straight,");
  gfx->setCursor(20, 105); gfx->print("then fine-tune trim[] in the code.");
  gfx->setCursor(20, 130); gfx->print("Press BOOT to start the robot.");
  gfx->flush();
  led(255, 200, 0);
  while (digitalRead(PIN_BTN) == LOW) delay(10);  // wait for release
  delay(100);
  while (digitalRead(PIN_BTN) == HIGH) delay(10); // wait for press
  while (digitalRead(PIN_BTN) == LOW) delay(10);
}

void setup() {
  Serial.begin(115200);
  analogWrite(PIN_BL, 255);
  pinMode(PIN_BTN, INPUT_PULLUP);
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  pinMode(PIN_TOUCH_HEAD, INPUT);
  pinMode(PIN_TOUCH_BACK, INPUT);
  for (int i = 0; i < 4; i++) ledcAttach(servoPin[i], 50, 14);

  if (!gfx->begin(80000000)) Serial.println("Display init failed");
  randomSeed(esp_random());

  if (digitalRead(PIN_BTN) == LOW) calibration();

  for (int i = 0; i < 4; i++) servoWrite(i, 90);
  Face::begin();
  Face::show(Face::HAPPY, 2000);
  Motion::push(Motion::UPDOWN, 1, 800, 1, 15);  // wake-up stretch
  lastInteract = millis();
  nextIdleAct = millis() + 10000;
}

void loop() {
  unsigned long now = millis();

  if (now - lastDist > 80) {
    lastDist = now;
    readDistance();
    static unsigned long lastLog = 0;
    if (now - lastLog > 1000) {
      lastLog = now;
      Serial.printf("dist=%.0fcm head=%d back=%d mode=%d\n", distCm, head.state, back.state, botMode);
    }
  }

  behaviour();
  Motion::update();

  if (now - lastFrame < 20) return;
  lastFrame = now;
  Face::frame();
  if (millis() < labelUntil) {
    gfx->setTextSize(2);
    int w = strlen(label) * 12;
    gfx->fillRoundRect(W / 2 - w / 2 - 8, 6, w + 16, 24, 6, 0x2104);
    gfx->setTextColor(0xFFFF);
    gfx->setCursor(W / 2 - w / 2, 11);
    gfx->print(label);
  }
  gfx->flush();
}
