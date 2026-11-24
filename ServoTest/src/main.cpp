// ServoTest - test up to 16 servos on a PCA9685 with a plain ESP32 (USB-C dev board).
// Control it from the serial monitor (115200 baud). Servos do NOT move at power-on.
//
// Wiring (see README.md):  ESP32 3V3 -> PCA9685 VCC,  GND -> GND,  GPIO21 -> SDA,  GPIO22 -> SCL
//                          separate 5-6 V supply -> PCA9685 V+ (green terminal), its GND joined with ESP32 GND
#include <Arduino.h>
#include <Wire.h>

// ---------------- settings ----------------
#ifndef PIN_SDA_CFG
#define PIN_SDA_CFG 21
#define PIN_SCL_CFG 22
#endif
static int PIN_SDA = PIN_SDA_CFG;           // may be swapped automatically if nothing answers
static int PIN_SCL = PIN_SCL_CFG;
static const uint8_t PCA_ADDR = 0x40;
#ifdef CHIKU_MAP
// Chiku robot wiring (PCA9685 channel numbers as printed on the board)
static const uint8_t TEST_CH[] = {1, 2, 3, 4, 5};
static const char* const NAMES[] = {"right foot", "right hip", "left foot", "left hip", "neck"};
#else
static const uint8_t TEST_CH[] = {0, 1, 2, 3, 4};
static const char* const NAMES[] = {"ch 0", "ch 1", "ch 2", "ch 3", "ch 4"};
#endif
static const int SERVO_COUNT = sizeof(TEST_CH);
static const int SWING = 30;              // test swing around the centre (degrees): gentle on the legs
static const int US_MIN = 500;            // pulse at 0 deg   (SG90 / MG90S: 500)
static const int US_MAX = 2400;           // pulse at 180 deg (SG90 / MG90S: 2400)
static const int SAFE_MIN = 20;           // test routines stay inside this angle range
static const int SAFE_MAX = 160;
static const float SPEED_DPS = 90.f;      // speed limit of every move (degrees per second)

// ---------------- PCA9685 ----------------
static const uint8_t MODE1 = 0x00, PRESCALE = 0xFE, LED0_ON_L = 0x06;
static bool chipOk = false;

static bool write8(uint8_t reg, uint8_t v) {
  Wire.beginTransmission(PCA_ADDR);
  Wire.write(reg); Wire.write(v);
  return Wire.endTransmission() == 0;
}

static bool chipInit() {
  if (!write8(MODE1, 0x10)) return false;          // sleep, so we may set the prescaler
  write8(PRESCALE, 121);                           // 25 MHz / (4096 * 50 Hz) - 1 = 121 -> 50 Hz
  write8(MODE1, 0x20);                             // wake, auto-increment
  delay(5);
  write8(MODE1, 0xA0);                             // restart
  return true;
}

static void pwmRaw(uint8_t ch, uint16_t on, uint16_t off) {
  Wire.beginTransmission(PCA_ADDR);
  Wire.write(LED0_ON_L + 4 * ch);
  Wire.write(on & 255); Wire.write(on >> 8);
  Wire.write(off & 255); Wire.write(off >> 8);
  Wire.endTransmission();
}

static void pulseUs(uint8_t ch, int us) {          // 20000 us period / 4096 steps
  pwmRaw(ch, 0, (uint16_t)(us * 4096L / 20000L));
}

static void channelOff(uint8_t ch) { pwmRaw(ch, 0, 0x1000); }   // "full off": servo goes limp

// ---------------- servo state ----------------
static float curDeg[16];
static bool known[16];                              // false until first command

static int degToUs(float d) { return US_MIN + (int)((US_MAX - US_MIN) * d / 180.f + 0.5f); }

// Move one servo smoothly (blocking) at SPEED_DPS. The first move after boot jumps to the start angle.
static void moveTo(uint8_t ch, float target) {
  target = constrain(target, 0.f, 180.f);
  if (!known[ch]) { known[ch] = true; curDeg[ch] = target; pulseUs(ch, degToUs(target)); return; }
  float step = SPEED_DPS * 0.02f;                   // 20 ms per tick
  while (fabsf(target - curDeg[ch]) > step) {
    curDeg[ch] += (target > curDeg[ch]) ? step : -step;
    pulseUs(ch, degToUs(curDeg[ch]));
    delay(20);
  }
  curDeg[ch] = target;
  pulseUs(ch, degToUs(target));
}

// ---------------- commands ----------------
static void scan() {
  Serial.println("I2C scan:");
  int n = 0;
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) { Serial.printf("  found 0x%02X%s\n", a, a == PCA_ADDR ? "  <- PCA9685" : a == 0x70 ? "  (all-call)" : ""); n++; }
  }
  if (!n) Serial.println("  nothing found: check SDA/SCL wires, 3V3 on VCC, common GND");
}

// 'x': try every SDA/SCL pair among the free GPIOs and report any I2C device that answers.
static void pinHunt() {
  static const int P[] = {1, 2, 3, 13, 16, 17, 18, 21, 43, 44, 46, 47, 48};
  Serial.println("pin hunt: trying every SDA/SCL pair on the free GPIOs (takes ~30 s)...");
  int found = 0;
  for (int a : P) for (int b : P) {
    if (a == b) continue;
    Wire.end();
    if (!Wire.begin(a, b, 100000)) continue;
    Wire.setTimeOut(5);
    for (uint8_t ad : {0x40, 0x41, 0x60, 0x70}) {
      Wire.beginTransmission(ad);
      if (Wire.endTransmission() == 0) { Serial.printf("  device 0x%02X with SDA=GPIO%d SCL=GPIO%d\n", ad, a, b); found++; }
    }
  }
  Wire.end();
  Wire.begin(PIN_SDA, PIN_SCL, 400000);
  Serial.printf("pin hunt done: %d hit(s)%s\n", found, found ? "" : " (no device on any pair: check power/GND/wires)");
}

static void help() {
  Serial.println(
      "\nServoTest commands\n"
      "  h            this help\n"
      "  s            scan the I2C bus (PCA9685 should be at 0x40)\n"
      "  x            hunt: try every SDA/SCL pair on the free GPIOs\n"
      "  c            centre all servos (90 deg), one after another\n"
      "  a <ch> <deg> move one servo, e.g. 'a 2 45'\n"
      "  p <ch> <us>  raw pulse in microseconds (500..2400), e.g. 'p 0 1500'\n"
      "  t            test: each servo in turn 90 -> 45 -> 135 -> 90\n"
      "  w            sweep: all servos together between 45 and 135, 3 times\n"
      "  o            all outputs off (servos go limp)\n"
      "Channels: 0 .. 15 on the PCA9685.");
  Serial.println("Test set:");
  for (int i = 0; i < SERVO_COUNT; i++) Serial.printf("  channel %d = %s\n", TEST_CH[i], NAMES[i]);
}

static void centreAll() {
  for (int i = 0; i < SERVO_COUNT; i++) { moveTo(TEST_CH[i], 90); delay(150); }   // staggered: no current spike
  Serial.println("centred");
}

static void testEach() {
  for (int i = 0; i < SERVO_COUNT; i++) {
    const uint8_t c = TEST_CH[i];
    Serial.printf("channel %d = %s: centre, -%d, +%d, centre\n", c, NAMES[i], SWING, SWING);
    moveTo(c, 90); delay(300);
    moveTo(c, 90 - SWING); delay(400);
    moveTo(c, 90 + SWING); delay(400);
    moveTo(c, 90); delay(300);
    channelOff(c); known[c] = false;
  }
  Serial.println("test done");
}

static void sweepAll() {
  for (int r = 0; r < 3; r++) {
    for (int i = 0; i < SERVO_COUNT; i++) moveTo(TEST_CH[i], 90 - SWING);
    for (int i = 0; i < SERVO_COUNT; i++) moveTo(TEST_CH[i], 90 + SWING);
  }
  centreAll();
}

static void allOff() {
  for (int i = 0; i < 16; i++) { channelOff(i); known[i] = false; }
  Serial.println("outputs off");
}

static void handle(String line) {
  line.trim();
  if (!line.length()) return;
  char cmd = line[0];
  int a = 0, b = 0;
  int n = sscanf(line.c_str() + 1, "%d %d", &a, &b);
  if (!chipOk && cmd != 's' && cmd != 'h' && cmd != 'x') { Serial.println("PCA9685 not found: run 's' and check wiring"); return; }
  switch (cmd) {
    case 'h': help(); break;
    case 's': scan(); break;
    case 'x': pinHunt(); break;
    case 'c': centreAll(); break;
    case 'a':
      if (n < 2 || a < 0 || a > 15) { Serial.println("usage: a <ch 0-15> <deg 0-180>"); break; }
      moveTo(a, b); Serial.printf("servo %d -> %d deg\n", a, b); break;
    case 'p':
      if (n < 2 || a < 0 || a > 15 || b < 400 || b > 2600) { Serial.println("usage: p <ch 0-15> <us 400-2600>"); break; }
      pulseUs(a, b); known[a] = false; Serial.printf("servo %d pulse %d us\n", a, b); break;
    case 't': testEach(); break;
    case 'w': sweepAll(); break;
    case 'o': allOff(); break;
    default: Serial.println("unknown command, 'h' for help");
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Wire.begin(PIN_SDA, PIN_SCL, 400000);
  chipOk = chipInit();
  if (!chipOk) {                                    // wires swapped? try the other way round
    Wire.end();
    Wire.begin(PIN_SCL, PIN_SDA, 400000);
    if (chipInit()) { chipOk = true; int t = PIN_SDA; PIN_SDA = PIN_SCL; PIN_SCL = t;
                      Serial.println("NOTE: SDA and SCL are swapped on your wiring (works this way round)"); }
    else { Wire.end(); Wire.begin(PIN_SDA, PIN_SCL, 400000); }
  }
  Serial.printf("\nServoTest: SDA=GPIO%d SCL=GPIO%d, PCA9685 0x%02X %s\n", PIN_SDA, PIN_SCL, PCA_ADDR,
                chipOk ? "found" : "NOT FOUND");
  if (chipOk) for (int i = 0; i < 16; i++) channelOff(i);   // start with every servo limp
  help();
}

void loop() {
  static String line;
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') { handle(line); line = ""; }
    else if (line.length() < 40) line += c;
  }
}
