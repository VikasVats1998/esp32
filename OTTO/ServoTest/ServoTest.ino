// Free servo tester  -  Waveshare ESP32-C6-LCD-1.47  (works on any ESP32 with the pins changed)
// Every channel starts FREE (no signal, servo limp). Control from the web UI (otto_server.py/servotest_server.py).
// 8 channels: the 4 Otto servo pins + 4 spare header pins, so a spare servo can be tested on ANY of them.
//   Commands (one per line, from the UI):
//     !u,ch,us            hold channel at a pulse width (500..2500 us, 1500 = 90deg)
//     !f,ch               free channel (ch = -1: all)
//     !v,speed            ramp speed in deg/s for all channels (0 = instant)
//     !s,ch,min,max,ms    sweep min..max degrees, full cycle every ms
//     !x,ch               stop sweep (hold current position)
// Telemetry 10 Hz:  S,speed, then per channel: target_us,current_us,measured_us   (0 = free / no pulse)
// Board: ESP32C6 Dev Module, USB CDC On Boot: Enabled.
#include "driver/gpio.h"

struct Ch { int pin; const char *name; float tgt; float cur; bool sweep; float mn, mx, per; uint32_t t0; int meas; };
Ch ch[8] = {
  {9,  "LEFT HIP",   0, 0, false, 0, 0, 0, 0, 0},
  {5,  "RIGHT HIP",  0, 0, false, 0, 0, 0, 0, 0},
  {17, "LEFT FOOT",  0, 0, false, 0, 0, 0, 0, 0},
  {16, "RIGHT FOOT", 0, 0, false, 0, 0, 0, 0, 0},
  {1,  "SPARE GPIO1", 0, 0, false, 0, 0, 0, 0, 0},
  {2,  "SPARE GPIO2", 0, 0, false, 0, 0, 0, 0, 0},
  {3,  "SPARE GPIO3", 0, 0, false, 0, 0, 0, 0, 0},
  {4,  "SPARE GPIO4", 0, 0, false, 0, 0, 0, 0, 0},
};
float speedDegS = 0;
const float US_PER_DEG = 2000.0f / 180.0f;

void out(int i, float us) {
  if (us <= 0) ledcWrite(ch[i].pin, 0);
  else ledcWrite(ch[i].pin, (uint32_t)(us * 16384 / 20000));   // 50 Hz, 14-bit
}

void cmd(const String &l) {
  char k = l.length() > 1 ? l[1] : 0;
  int a = -9, b = 0, c = 0, d = 0;
  int n = sscanf(l.c_str() + 2, ",%d,%d,%d,%d", &a, &b, &c, &d);
  if (k == 'v') { speedDegS = max(0, a); return; }
  if (k == 'f' && a == -1) { for (int i = 0; i < 8; i++) { ch[i].tgt = 0; ch[i].cur = 0; ch[i].sweep = false; out(i, 0); } return; }
  if (n < 1 || a < 0 || a > 7) return;
  Ch &h = ch[a];
  if (k == 'u' && n >= 2) { h.sweep = false; h.tgt = constrain(b, 500, 2500); if (h.cur <= 0 || speedDegS == 0) h.cur = h.tgt; }
  else if (k == 'f') { h.tgt = 0; h.cur = 0; h.sweep = false; out(a, 0); }
  else if (k == 's' && n >= 4) {
    h.mn = constrain(b, 0, 180); h.mx = constrain(c, 0, 180); h.per = max(300, d);
    h.t0 = millis(); h.sweep = true; if (h.cur <= 0) h.cur = 500 + (h.mn + h.mx) / 2 * US_PER_DEG;
  }
  else if (k == 'x') { h.sweep = false; h.tgt = h.cur; }
}

void readSerial() {
  static String line; static bool in = false;
  while (Serial.available()) {
    char c = Serial.read();
    if (in) { if (c == '\n') { cmd(line); in = false; } else if (c != '\r' && line.length() < 48) line += c; continue; }
    if (c == '!') { in = true; line = "!"; }
  }
}

void setup() {
  Serial.begin(115200);
  for (int i = 0; i < 8; i++) { ledcAttach(ch[i].pin, 50, 14); ledcWrite(ch[i].pin, 0); gpio_input_enable((gpio_num_t)ch[i].pin); }
  Serial.println("ServoTest ready - all channels FREE");
}

void loop() {
  readSerial();
  static uint32_t tStep = 0;
  uint32_t now = millis();
  if (now - tStep >= 10) {
    float dt = (now - tStep) / 1000.0f; tStep = now;
    for (int i = 0; i < 8; i++) {
      Ch &h = ch[i];
      if (h.sweep) {
        float mid = (h.mn + h.mx) / 2, amp = (h.mx - h.mn) / 2;
        h.tgt = 500 + (mid + amp * sin(2 * PI * (now - h.t0) / h.per)) * US_PER_DEG;
        h.cur = h.tgt;                                    // sweep is its own motion; no ramp
      } else if (h.tgt > 0) {
        if (speedDegS <= 0) h.cur = h.tgt;
        else { float step = speedDegS * US_PER_DEG * dt; h.cur += constrain(h.tgt - h.cur, -step, step); }
      }
      out(i, h.tgt > 0 ? h.cur : 0);
    }
  }

  static uint32_t tMeas = 0; static int mi = 0;           // read back the real pulse, one channel per 60 ms
  if (now - tMeas > 60) {
    tMeas = now;
    ch[mi].meas = ch[mi].tgt > 0 ? (int)pulseIn(ch[mi].pin, HIGH, 25000) : 0;
    mi = (mi + 1) % 8;
  }

  static uint32_t tTel = 0;
  if (now - tTel > 100) {
    tTel = now;
    Serial.printf("S,%.0f", speedDegS);
    for (int i = 0; i < 8; i++) Serial.printf(",%.0f,%.0f,%d", ch[i].tgt, ch[i].tgt > 0 ? ch[i].cur : 0, ch[i].meas);
    Serial.println();
  }
}
