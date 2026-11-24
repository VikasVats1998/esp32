// Otto calibration + hardware test  -  Waveshare ESP32-C6-LCD-1.47
// Pins follow OTTO/PINS.md. The BOOT button is NOT usable (GPIO9 = servo #4), so the 4 TTP223 touch pads are the buttons:
//
//   Pad 1 (GPIO1)  tap = next servo        hold = next page   (CALIB -> SENSORS -> SWEEP)
//   Pad 2 (GPIO2)  hold = trim +           (tap +1, hold repeats)
//   Pad 3 (GPIO3)  hold = trim -
//   Pad 4 (GPIO4)  tap = toggle reverse    hold 1 s = SAVE to flash
//
// Pages: CALIB  - all servos at 90deg, adjust trim/reverse of the selected servo until the legs stand straight
//        SENSORS - live touch pads + ultrasonic distance
//        SWEEP  - selected servo swings +-30deg so you can check direction/mirroring
// Saved values are stored in NVS and printed on Serial as ready-to-paste C arrays.
// Board: ESP32C6 Dev Module, USB CDC On Boot: Enabled. Library: GFX Library for Arduino.
#include <Arduino_GFX_Library.h>
#include <Preferences.h>
#include "driver/gpio.h"

// ---- Display (fixed on board) ----
#define PIN_MOSI 6
#define PIN_SCLK 7
#define PIN_CS   14
#define PIN_DC   15
#define PIN_RST  21
#define PIN_BL   22

// ---- Otto pins (OTTO/PINS.md) ----
#define PIN_TRIG 23
#define PIN_ECHO 0
const int touchPin[4] = {1, 2, 3, 4};
const int servoPin[4] = {9, 5, 17, 16};
const char *servoName[4] = {"LEFT HIP", "RIGHT HIP", "LEFT FOOT", "RIGHT FOOT"};

// Touch pads not wired yet? Leave false and control everything from the laptop over USB serial
// (Serial Monitor 115200, or: python3 -m serial.tools.miniterm /dev/ttyACM0 115200).
//   n next servo   + / -  trim +-1    ] / [  trim +-5    r reverse    w save    p next page    c all to 90
const bool USE_TOUCH = false;

#define W 320
#define H 172
#define BLACK  0x0000
#define WHITE  0xFFFF
#define YELLOW 0xFFE0
#define GREEN  0x07E0
#define RED    0xF800
#define CYAN   0x07FF
#define GREY   0x7BEF
#define DARK   0x2104
#define BLUE   0x041F

Arduino_DataBus *bus = new Arduino_ESP32SPI(PIN_DC, PIN_CS, PIN_SCLK, PIN_MOSI, GFX_NOT_DEFINED);
Arduino_GFX *panel = new Arduino_ST7789(bus, PIN_RST, 1, true, 172, 320, 34, 0, 34, 0);
Arduino_Canvas *gfx = new Arduino_Canvas(W, H, panel);
Preferences prefs;

int  trim[4] = {0, 0, 0, 0};
bool rev[4]  = {false, false, false, false};
float manual[4] = {-1, -1, -1, -1};   // live test angle from the web UI (-1 = FREE: no signal, servo is limp and can be turned by hand)
int  sel = 0, page = 0;
bool dirty = false;
uint32_t savedFlash = 0;

// ---------------- servos ----------------
void servoWrite(int i, float deg) {
  deg = constrain(deg, 0, 180);
  float d = rev[i] ? 180 - deg : deg;
  d = constrain(d + trim[i], 0, 180);
  uint32_t us = 500 + d * 2000 / 180;
  ledcWrite(servoPin[i], us * 16384 / 20000);   // 50 Hz, 14-bit
}
void servoFree(int i) { ledcWrite(servoPin[i], 0); }          // stop pulses: servo goes limp
void allFree()   { for (int i = 0; i < 4; i++) { manual[i] = -1; servoFree(i); } }
void allCentre() { for (int i = 0; i < 4; i++) { manual[i] = 90; servoWrite(i, 90); } }   // explicit only
void applyServos() { for (int i = 0; i < 4; i++) { if (manual[i] >= 0) servoWrite(i, manual[i]); else servoFree(i); } }

// ---------------- storage ----------------
void load() {
  prefs.begin("otto", true);
  uint8_t mask = prefs.getUChar("rev", 0);
  for (int i = 0; i < 4; i++) { trim[i] = prefs.getInt((String("t") + i).c_str(), 0); rev[i] = mask >> i & 1; }
  prefs.end();
}
void save() {
  prefs.begin("otto", false);
  uint8_t mask = 0;
  for (int i = 0; i < 4; i++) { prefs.putInt((String("t") + i).c_str(), trim[i]); mask |= rev[i] << i; }
  prefs.putUChar("rev", mask);
  prefs.end();
  dirty = false; savedFlash = millis();
  Serial.printf("\n// SAVED - paste into your sketch:\nint  trim[4]    = {%d, %d, %d, %d};\nbool reverse[4] = {%s, %s, %s, %s};\n",
                trim[0], trim[1], trim[2], trim[3], rev[0] ? "true" : "false", rev[1] ? "true" : "false",
                rev[2] ? "true" : "false", rev[3] ? "true" : "false");
}

// ---------------- touch pads ----------------
struct Pad { bool down = false, longDone = false; uint32_t t = 0, rep = 0; } pad[4];
bool tapped[4], heldRep[4], longPress[4];

// Line commands (from the web UI):  !t,i,val  trim | !r,i,0/1 reverse | !a,i,deg live angle (-1 = release)
//                                    !s,i select  | !w save | !c centre all | !p page
void lineCmd(const String &l) {
  int a = 0, b = 0; int n = sscanf(l.c_str() + 1, "%*c,%d,%d", &a, &b);
  char k = l.length() > 1 ? l[1] : 0;
  if (k == 'w') { longPress[3] = true; return; }
  if (k == 'c') { allCentre(); return; }
  if (k == 'f') { allFree(); return; }
  if (k == 'p') { longPress[0] = true; return; }
  if (n < 1 || a < 0 || a > 3) return;
  if (k == 's') sel = a;
  else if (k == 't' && n == 2) { trim[a] = constrain(b, -45, 45); dirty = true; }
  else if (k == 'r' && n == 2) { rev[a] = b; dirty = true; }
  else if (k == 'a' && n == 2) manual[a] = b < 0 ? -1 : constrain(b, 0, 180);
}

void readSerialCmds() {
  static String line; static bool inLine = false;
  while (Serial.available()) {
    char c = Serial.read();
    if (inLine) { if (c == '\n') { lineCmd(line); inLine = false; } else if (c != '\r' && line.length() < 40) line += c; continue; }
    if (c == '!') { inLine = true; line = "!"; continue; }
    switch (c) {
      case 'n': tapped[0] = true; break;
      case 'p': longPress[0] = true; break;
      case '+': case '=': tapped[1] = true; break;
      case '-': tapped[2] = true; break;
      case ']': for (int k = 0; k < 5; k++) { trim[sel] = constrain(trim[sel] + 1, -45, 45); } dirty = true; break;
      case '[': for (int k = 0; k < 5; k++) { trim[sel] = constrain(trim[sel] - 1, -45, 45); } dirty = true; break;
      case 'r': tapped[3] = true; break;
      case 'w': longPress[3] = true; break;
      case 'c': allCentre(); break;
      case 'f': allFree(); break;
    }
  }
}

void readPads() {
  for (int i = 0; i < 4; i++) tapped[i] = heldRep[i] = longPress[i] = false;
  readSerialCmds();
  if (!USE_TOUCH) return;
  for (int i = 0; i < 4; i++) {
    bool d = digitalRead(touchPin[i]) == HIGH;
    uint32_t now = millis();
    if (d && !pad[i].down) { pad[i].down = true; pad[i].longDone = false; pad[i].t = now; pad[i].rep = now + 450; }
    if (d && pad[i].down) {
      if (!pad[i].longDone && now - pad[i].t > 1000) { pad[i].longDone = true; longPress[i] = true; }
      if (now >= pad[i].rep && now - pad[i].t > 450) { heldRep[i] = true; pad[i].rep = now + 70; }
    }
    if (!d && pad[i].down) {
      pad[i].down = false;
      if (!pad[i].longDone && now - pad[i].t < 450) tapped[i] = true;
    }
  }
}

// ---------------- ultrasonic ----------------
float distanceCm() {
  digitalWrite(PIN_TRIG, LOW); delayMicroseconds(3);
  digitalWrite(PIN_TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  unsigned long us = pulseIn(PIN_ECHO, HIGH, 25000);
  return us ? us / 58.0f : -1;
}

// ---------------- drawing ----------------
void header(const char *title, int pg) {
  gfx->fillRect(0, 0, W, 22, DARK);
  gfx->setTextColor(YELLOW); gfx->setTextSize(2); gfx->setCursor(6, 4); gfx->print(title);
  for (int i = 0; i < 3; i++) gfx->fillCircle(W - 52 + i * 14, 11, 4, i == pg ? YELLOW : GREY);
  if (dirty) { gfx->setTextColor(RED); gfx->setTextSize(1); gfx->setCursor(W - 100, 8); gfx->print("*"); }
  if (millis() - savedFlash < 1500) { gfx->setTextColor(GREEN); gfx->setTextSize(1); gfx->setCursor(W - 150, 8); gfx->print("SAVED"); }
}
void footer(const char *s) {
  gfx->fillRect(0, H - 14, W, 14, DARK);
  gfx->setTextColor(CYAN); gfx->setTextSize(1); gfx->setCursor(4, H - 11); gfx->print(s);
}

void drawCalib() {
  header("CALIB", 0);
  // selected servo big panel
  gfx->setTextColor(WHITE); gfx->setTextSize(2); gfx->setCursor(8, 28);
  gfx->printf("%d/4 %s", sel + 1, servoName[sel]);
  gfx->setTextColor(GREY); gfx->setTextSize(1); gfx->setCursor(8, 48); if (manual[sel] >= 0) gfx->printf("GPIO%d  HOLD %.0f deg", servoPin[sel], manual[sel]); else gfx->printf("GPIO%d  FREE (limp)", servoPin[sel]);
  gfx->setTextColor(trim[sel] ? YELLOW : GREEN); gfx->setTextSize(5); gfx->setCursor(8, 62);
  gfx->printf("%+d", trim[sel]);
  gfx->setTextSize(2); gfx->setTextColor(GREY); gfx->setCursor(8 + 5 * 6 * 4 + 12, 80); gfx->print("trim deg");
  gfx->setTextColor(rev[sel] ? RED : GREY); gfx->setTextSize(2); gfx->setCursor(8, 108);
  gfx->printf("REVERSE: %s", rev[sel] ? "YES" : "no");

  // 4-servo overview on the right
  for (int i = 0; i < 4; i++) {
    int y = 28 + i * 24;
    gfx->fillRoundRect(214, y, 100, 21, 4, i == sel ? BLUE : DARK);
    gfx->setTextSize(1); gfx->setTextColor(WHITE); gfx->setCursor(220, y + 3); gfx->print(servoName[i]);
    gfx->setTextColor(trim[i] || rev[i] ? YELLOW : GREY); gfx->setCursor(220, y + 12);
    gfx->printf("%+d%s", trim[i], rev[i] ? " REV" : "");
  }
  gfx->setTextColor(GREY); gfx->setTextSize(1); gfx->setCursor(8, 135);
  gfx->print("Servos stay FREE until you set an angle in the web UI.");
  footer(USE_TOUCH ? "[1]next servo [2]+ [3]- [4]rev  hold1:page hold4:SAVE" : "USB serial: n next  + -  [ ]  r rev  w SAVE  p page");
}

void drawSensors(float cm) {
  header("SENSORS", 1);
  const char *nm[4] = {"HEAD", "BACK", "LEFT", "RIGHT"};
  for (int i = 0; i < 4; i++) {
    bool on = digitalRead(touchPin[i]) == HIGH;
    int x = 12 + i * 76;
    gfx->fillCircle(x + 30, 55, 24, on ? GREEN : DARK);
    gfx->drawCircle(x + 30, 55, 24, GREY);
    gfx->setTextColor(on ? BLACK : WHITE); gfx->setTextSize(2); gfx->setCursor(x + 24, 48); gfx->print(i + 1);
    gfx->setTextColor(GREY); gfx->setTextSize(1); gfx->setCursor(x + 12, 86); gfx->printf("%s GPIO%d", nm[i], touchPin[i]);
  }
  gfx->setTextColor(WHITE); gfx->setTextSize(2); gfx->setCursor(12, 108);
  if (cm < 0) { gfx->setTextColor(RED); gfx->print("Ultrasonic: no echo"); }
  else gfx->printf("Distance: %.0f cm", cm);
  gfx->drawRect(12, 130, 296, 12, GREY);
  if (cm > 0) gfx->fillRect(13, 131, constrain((int)(cm / 200.0f * 294), 1, 294), 10, cm < 15 ? RED : GREEN);
  footer(USE_TOUCH ? "TRIG=23 ECHO=0 (1k/2k divider!)  hold pad1: page" : "TRIG=23 ECHO=0 (1k/2k divider!)   serial p: page");
}

uint32_t sweepT0 = 0;
void drawSweep(float ang) {
  header("SWEEP", 2);
  gfx->setTextColor(WHITE); gfx->setTextSize(2); gfx->setCursor(8, 28); gfx->printf("%d/4 %s", sel + 1, servoName[sel]);
  gfx->setTextColor(GREY); gfx->setTextSize(1); gfx->setCursor(8, 48); gfx->printf("GPIO%d  other servos held at 90", servoPin[sel]);
  gfx->setTextColor(YELLOW); gfx->setTextSize(5); gfx->setCursor(8, 66); gfx->printf("%3.0f", ang);
  gfx->setTextSize(2); gfx->setCursor(112, 86); gfx->print("deg");
  int cx = 240, cy = 130, r = 55;                       // dial
  gfx->drawCircle(cx, cy, r, GREY);
  float a = (ang - 90) * DEG_TO_RAD;
  gfx->drawLine(cx, cy, cx + sin(a) * r, cy - cos(a) * r, GREEN);
  gfx->fillCircle(cx, cy, 4, GREEN);
  gfx->setTextColor(GREY); gfx->setTextSize(1); gfx->setCursor(8, 125);
  gfx->print("Left & right legs should mirror each other.");
  footer(USE_TOUCH ? "[1]next servo  [4]reverse   hold1: next page" : "serial: n next servo  r reverse  p page");
}

// ---------------- setup / loop ----------------
void setup() {
  Serial.begin(115200);
  pinMode(PIN_TRIG, OUTPUT); pinMode(PIN_ECHO, INPUT);
  for (int i = 0; i < 4; i++) pinMode(touchPin[i], USE_TOUCH ? INPUT : INPUT_PULLDOWN);  // pulldown: unconnected pads never read as touched
  analogWrite(PIN_BL, 255);
  if (!gfx->begin(80000000)) Serial.println("Display init failed");
  load();
  for (int i = 0; i < 4; i++) ledcAttach(servoPin[i], 50, 14);
  for (int i = 0; i < 4; i++) gpio_input_enable((gpio_num_t)servoPin[i]);   // lets us read back our own PWM pulse
  allFree();                      // boot with every servo limp (no signal)
  Serial.println("Otto calibration ready. Keys: n next servo, + - trim, [ ] trim x5, r reverse, w save, p page, c centre all.");
}

void loop() {
  readPads();

  if (longPress[0]) page = (page + 1) % 3;
  else if (tapped[0]) sel = (sel + 1) % 4;
  if (tapped[3] || (page != 0 && false)) { rev[sel] = !rev[sel]; dirty = true; }
  if (longPress[3]) save();
  if (page == 0) {
    int d = 0;
    if (tapped[1] || heldRep[1]) d += 1;
    if (tapped[2] || heldRep[2]) d -= 1;
    if (d) { trim[sel] = constrain(trim[sel] + d, -45, 45); dirty = true; }
  }

  float ang = 90;
  static uint32_t tDist = 0; static float cm = -1;
  if (millis() - tDist > 120) { tDist = millis(); cm = distanceCm(); }
  if (page == 2) {
    ang = 90 + 30 * sin((millis() - sweepT0) / 700.0f);
    applyServos();
    servoWrite(sel, ang);
  } else { sweepT0 = millis(); applyServos(); }

  // measure the real pulse width on one held servo pin per cycle (0 = no pulse / free)
  static int meas[4] = {0, 0, 0, 0}; static int mi = 0; static uint32_t tMeas = 0;
  if (millis() - tMeas > 60) {
    tMeas = millis();
    meas[mi] = manual[mi] >= 0 ? (int)pulseIn(servoPin[mi], HIGH, 25000) : 0;
    mi = (mi + 1) % 4;
  }

  static uint32_t lastTel = 0;
  if (millis() - lastTel > 100) {
    lastTel = millis();
    int rm = 0, tm = 0;
    for (int i = 0; i < 4; i++) { rm |= rev[i] << i; tm |= (digitalRead(touchPin[i]) == HIGH) << i; }
    Serial.printf("T,%d,%d,%d,%d,%d,%d,%d,%d,%.1f,%d,%.0f,%.0f,%.0f,%.0f,%d,%d,%d,%d\n", sel, page, trim[0], trim[1], trim[2], trim[3],
                  rm, tm, cm, dirty, manual[0], manual[1], manual[2], manual[3], meas[0], meas[1], meas[2], meas[3]);
  }

  static uint32_t lastDraw = 0;
  if (millis() - lastDraw > 40) {
    lastDraw = millis();
    gfx->fillScreen(BLACK);
    if (page == 0) drawCalib(); else if (page == 1) drawSensors(cm); else drawSweep(ang);
    gfx->flush();
  }
}
