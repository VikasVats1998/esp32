// height.cpp - see height.h
#include "height.h"
#include "config.h"
#include "logbuf.h"
#include "storage.h"

namespace height {

static float est = 75.0f;
static bool estValid = false;
static float serialCm = 0;
static uint32_t serialAt = 0;
static uint32_t lastChange = 0;
static char display[8] = "";
static uint32_t frames = 0, badFrames = 0, byteCount = 0;

// capture ring
static const int LINES = 20, LINE_LEN = 56;
static char ring[LINES][LINE_LEN];
static int ringCount = 0;

static void capture(const char* tag, const uint8_t* d, size_t n) {
  char* l = ring[ringCount % LINES];
  int p = snprintf(l, LINE_LEN, "%s", tag);
  for (size_t i = 0; i < n && p < LINE_LEN - 4; i++) p += snprintf(l + p, LINE_LEN - p, " %02X", d[i]);
  ringCount++;
}

size_t rawText(char* out, size_t n) {
  size_t p = 0;
  int first = ringCount > LINES ? ringCount - LINES : 0;
  for (int i = first; i < ringCount && p + LINE_LEN < n; i++) p += snprintf(out + p, n - p, "%s\n", ring[i % LINES]);
  return p;
}

void init() {
  est = storage::get().estOffsetCm;
  if (PIN_DESK_RX >= 0) Serial1.begin(DESK_BAUD, SERIAL_8N1, PIN_DESK_RX, -1);
}

// ---- frame decoding ----
static char seg7(uint8_t b) {
  switch (b & 0x7F) {
    case 0x3F: return '0'; case 0x06: return '1'; case 0x5B: return '2'; case 0x4F: return '3';
    case 0x66: return '4'; case 0x6D: return '5'; case 0x7D: return '6'; case 0x07: return '7';
    case 0x7F: return '8'; case 0x6F: return '9'; case 0x00: return ' '; case 0x40: return '-';
    case 0x79: return 'E'; case 0x50: return 'r'; case 0x77: return 'A'; case 0x3E: return 'U';
    case 0x38: return 'L'; case 0x76: return 'H'; case 0x5E: return 'd'; case 0x54: return 'n';
    case 0x5C: return 'o'; case 0x73: return 'P';
    default: return '?';
  }
}

static void setHeight(float cm) {
  if (cm < 30 || cm > 160) return;                         // not a plausible desk height
  if (fabsf(cm - serialCm) >= 0.1f) lastChange = millis();
  serialCm = cm;
  serialAt = millis();
  est = cm;                                                // the estimator follows the real value
  estValid = true;
}

static void handleFrame(uint8_t cmd, const uint8_t* p, uint8_t n) {
  frames++;
  if (cmd == 0x01 && n >= 2) {                             // height as a 16 bit number (mm, i.e. 0.1 cm units)
    uint16_t v = (p[0] << 8) | p[1];
    if (v >= 400 && v <= 1600) setHeight(v / 10.0f);
  } else if (cmd == 0x12 && n >= 3) {                      // the three 7-segment digits of the display
    char t[8];
    int k = 0;
    for (int i = 0; i < 3 && k < 6; i++) { t[k++] = seg7(p[i]); if (p[i] & 0x80) t[k++] = '.'; }
    t[k] = 0;
    strlcpy(display, t, sizeof(display));
    char* s = t;
    while (*s == ' ') s++;
    bool numeric = *s != 0;
    for (char* c = s; *c; c++) if (!((*c >= '0' && *c <= '9') || *c == '.')) numeric = false;
    if (numeric) setHeight(atof(s));
  }
}

void update() {
  static uint8_t buf[24];
  static int n = 0;
  static uint8_t noise[12];
  static int noiseN = 0;
  while (PIN_DESK_RX >= 0 && Serial1.available()) {
    uint8_t b = Serial1.read();
    byteCount++;
    if (n == 0 && b != 0xF2) {                             // not in a frame: remember bytes for the capture tool
      noise[noiseN++] = b;
      if (noiseN == (int)sizeof(noise)) { capture("raw", noise, noiseN); noiseN = 0; }
      continue;
    }
    if (n == 0 && noiseN) { capture("raw", noise, noiseN); noiseN = 0; }
    buf[n++] = b;
    if (n == 2 && buf[1] != 0xF2) { n = 0; continue; }
    if (n >= 4) {
      uint8_t len = buf[3];
      if (len > 16) { n = 0; continue; }
      if (n == 4 + len + 2) {                              // F2 F2 cmd len data.. cs 7E
        uint8_t sum = buf[2] + buf[3];
        for (int i = 0; i < len; i++) sum += buf[4 + i];
        bool good = sum == buf[4 + len] && buf[5 + len] == 0x7E;
        capture(good ? "ok " : "bad", buf, n);
        if (good) handleFrame(buf[2], buf + 4, len); else badFrames++;
        n = 0;
      }
    }
    if (n >= (int)sizeof(buf)) n = 0;
  }
}

void noteMotion(int dir, uint32_t dtMs) {
  if (!dir || dtMs == 0 || dtMs > 200) return;
  est = constrain(est + dir * EST_SPEED_CM_S * dtMs / 1000.0f, HEIGHT_MIN_CM, HEIGHT_MAX_CM);
  lastChange = millis();
}

void calibrate(float cm) { est = constrain(cm, HEIGHT_MIN_CM, HEIGHT_MAX_CM); estValid = true; LOGI("height", "calibrated to %.1f cm", cm); }
void invalidateEstimate() { if (millis() - serialAt > SERIAL_FRESH_MS) estValid = false; }
float estimate() { return est; }

Info get() {
  Info i = {};
  bool fresh = serialAt && millis() - serialAt < SERIAL_FRESH_MS;
  if (fresh) { i.cm = serialCm; i.valid = true; i.src = "display"; }
  else if (estValid) { i.cm = est; i.valid = true; i.src = "estimate"; }
  else { i.cm = 0; i.valid = false; i.src = "none"; }
  i.moving = millis() - lastChange < 700;
  strlcpy(i.display, display, sizeof(i.display));
  i.frames = frames;
  i.badFrames = badFrames;
  i.bytes = byteCount;
  return i;
}

}  // namespace height
