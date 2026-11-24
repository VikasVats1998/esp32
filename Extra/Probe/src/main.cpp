// Probe - read-only measuring tool (ESP32-S3 Super Mini). Serial console, 115200 baud, one answer line per item,
// easy to parse by Bringup/bringup.py ("probe" command).
//
// SAFETY: connect a probe wire only through a 10 k ohm resistor, and ONLY to signals of at most 3.3 V
// (use a divider for 5 V signals). Always connect GND first. Never connect mains or a battery directly.
//
//   help                        this text
//   pins                        pins that can be probed
//   adc                         voltage (mV) on every probe pin
//   watch <pin> [ms]            min / max / average mV over time (finds moving potentiometers, pulsing lines)
//   read <pin>                  digital level
//   pull <pin> up|down|none     internal pull resistor
//   edges <pin> [ms]            number of edges, frequency, shortest high / low pulse (finds data lines and PWM)
//   baud <pin> [ms]             guess the serial speed of a data line from the shortest pulse
//   uart <pin> <baud> [ms]      dump the bytes of a serial line (8N1) as hex and text
#include <Arduino.h>

static const int PINS[] = {1, 2, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13};
static const int NPINS = sizeof(PINS) / sizeof(PINS[0]);

static bool okPin(int p) { for (int q : PINS) if (q == p) return true; return false; }

static volatile uint32_t stamps[4096];
static volatile int nStamps = 0;
static volatile bool levelAfter[4096];
static int edgePin = -1;
static void IRAM_ATTR onEdgePin() {
  int i = nStamps;
  if (i < 4096) { levelAfter[i] = gpio_get_level((gpio_num_t)edgePin); stamps[i] = micros(); nStamps = i + 1; }
}

static void captureEdges(int pin, uint32_t ms) {
  nStamps = 0;
  edgePin = pin;
  pinMode(pin, INPUT);
  attachInterrupt(digitalPinToInterrupt(pin), onEdgePin, CHANGE);
  delay(ms);
  detachInterrupt(digitalPinToInterrupt(pin));
}

static void cmdEdges(int pin, uint32_t ms, bool baudGuess) {
  captureEdges(pin, ms);
  int n = nStamps;
  uint32_t minHigh = 0xFFFFFFFF, minLow = 0xFFFFFFFF, minAny = 0xFFFFFFFF;
  for (int i = 1; i < n; i++) {
    uint32_t w = stamps[i] - stamps[i - 1];
    if (w == 0) continue;
    bool high = levelAfter[i - 1];                 // level held between edge i-1 and edge i
    if (high) minHigh = min(minHigh, w); else minLow = min(minLow, w);
    minAny = min(minAny, w);
  }
  if (baudGuess) {
    if (n < 4) { Serial.printf("baud pin=%d edges=%d guess=none (no activity)\n", pin, n); return; }
    static const int STD[] = {1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400, 460800};
    double bit = minAny;                          // microseconds of the shortest pulse = one bit time
    int best = STD[0];
    for (int b : STD) if (fabs(1e6 / b - bit) < fabs(1e6 / best - bit)) best = b;
    Serial.printf("baud pin=%d edges=%d shortest_us=%u guess=%d\n", pin, n, (unsigned)minAny, best);
    return;
  }
  double hz = n > 1 ? (double)(n / 2) * 1e6 / (double)(stamps[n - 1] - stamps[0] + 1) : 0;
  Serial.printf("edges pin=%d count=%d window_ms=%u rate_hz=%.1f min_high_us=%u min_low_us=%u%s\n", pin, n, (unsigned)ms, hz,
                minHigh == 0xFFFFFFFF ? 0 : (unsigned)minHigh, minLow == 0xFFFFFFFF ? 0 : (unsigned)minLow, n >= 4096 ? " (buffer full)" : "");
}

static void cmdUart(int pin, uint32_t baud, uint32_t ms) {
  Serial1.end();
  Serial1.setRxBufferSize(2048);
  Serial1.begin(baud, SERIAL_8N1, pin, -1);
  uint32_t t0 = millis();
  String hex, txt;
  int count = 0;
  while (millis() - t0 < ms) {
    while (Serial1.available()) {
      uint8_t b = Serial1.read();
      char h[4];
      snprintf(h, sizeof(h), "%02X ", b);
      if (count < 400) { hex += h; txt += (b >= 32 && b < 127) ? (char)b : '.'; }
      count++;
    }
    delay(1);
  }
  Serial1.end();
  Serial.printf("uart pin=%d baud=%u bytes=%d\n", pin, (unsigned)baud, count);
  if (count) { Serial.printf("hex %s\n", hex.c_str()); Serial.printf("txt %s\n", txt.c_str()); }
}

static void handle(String line) {
  line.trim();
  if (!line.length()) return;
  char cmd[12] = "";
  int a = -1, b = -1, c = -1;
  char s[8] = "";
  sscanf(line.c_str(), "%11s", cmd);
  String c0 = cmd;
  if (c0 == "help") {
    Serial.println("probe: help | pins | adc | watch <pin> [ms] | read <pin> | pull <pin> up|down|none | edges <pin> [ms] | baud <pin> [ms] | uart <pin> <baud> [ms]");
  } else if (c0 == "pins") {
    Serial.print("pins");
    for (int p : PINS) Serial.printf(" %d", p);
    Serial.println();
  } else if (c0 == "adc") {
    analogReadResolution(12);
    for (int p : PINS) {
      pinMode(p, INPUT);
      long sum = 0;
      for (int i = 0; i < 16; i++) sum += analogReadMilliVolts(p);
      Serial.printf("adc pin=%d mv=%ld\n", p, sum / 16);
    }
    Serial.println("adc done (a floating pin reads noise)");
  } else if (c0 == "watch" && sscanf(line.c_str(), "%*s %d %d", &a, &b) >= 1 && okPin(a)) {
    if (b <= 0) b = 2000;
    int mn = 5000, mx = 0;
    long sum = 0, n = 0;
    pinMode(a, INPUT);
    uint32_t t0 = millis();
    while (millis() - t0 < (uint32_t)b) { int mv = analogReadMilliVolts(a); mn = min(mn, mv); mx = max(mx, mv); sum += mv; n++; delay(2); }
    Serial.printf("watch pin=%d ms=%d min=%d max=%d avg=%ld samples=%ld\n", a, b, mn, mx, sum / max(n, 1L), n);
  } else if (c0 == "read" && sscanf(line.c_str(), "%*s %d", &a) == 1 && okPin(a)) {
    pinMode(a, INPUT);
    Serial.printf("read pin=%d level=%d\n", a, digitalRead(a));
  } else if (c0 == "pull" && sscanf(line.c_str(), "%*s %d %7s", &a, s) == 2 && okPin(a)) {
    String m = s;
    pinMode(a, m == "up" ? INPUT_PULLUP : m == "down" ? INPUT_PULLDOWN : INPUT);
    Serial.printf("pull pin=%d mode=%s\n", a, s);
  } else if (c0 == "edges" && sscanf(line.c_str(), "%*s %d %d", &a, &b) >= 1 && okPin(a)) {
    cmdEdges(a, b > 0 ? b : 1000, false);
  } else if (c0 == "baud" && sscanf(line.c_str(), "%*s %d %d", &a, &b) >= 1 && okPin(a)) {
    cmdEdges(a, b > 0 ? b : 2000, true);
  } else if (c0 == "uart" && sscanf(line.c_str(), "%*s %d %d %d", &a, &b, &c) >= 2 && okPin(a) && b > 0) {
    cmdUart(a, b, c > 0 ? c : 2000);
  } else {
    Serial.println("error: unknown command or bad pin (type help, pins)");
  }
}

static String line;
void setup() {
  Serial.begin(115200);
  Serial.setTxTimeoutMs(100);
  delay(300);
  Serial.println("probe ready (type help)");
}

void loop() {
  while (Serial.available()) {
    char ch = Serial.read();
    if (ch == '\n' || ch == '\r') { handle(line); line = ""; }
    else if (line.length() < 80) line += ch;
  }
  delay(1);
}
