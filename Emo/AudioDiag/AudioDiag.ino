// AudioDiag - find a working MAX98357A wiring
// Cycles through every BCLK/LRC/DIN combination of the I2S pins and plays a beep pattern.
// When you hear sound, press BOOT to lock that combination (press again to continue).

#include <Arduino_GFX_Library.h>
#include <ESP_I2S.h>

#define PIN_MOSI 6
#define PIN_SCLK 7
#define PIN_CS   14
#define PIN_DC   15
#define PIN_RST  21
#define PIN_BL   22
#define PIN_BTN  9

#define RATE 16000
#define VOLUME 0.25f

struct Combo { int bclk, lrc, din; };
const Combo combos[] = {
  {4, 5, 17}, {5, 4, 17}, {4, 17, 5}, {17, 4, 5}, {5, 17, 4}, {17, 5, 4},  // all orders of 4/5/17
  {4, 5, 16}, {5, 4, 16},                                                  // DIN on TX pin by mistake
};
const int N = sizeof(combos) / sizeof(combos[0]);

Arduino_DataBus *bus = new Arduino_ESP32SPI(PIN_DC, PIN_CS, PIN_SCLK, PIN_MOSI, GFX_NOT_DEFINED);
Arduino_GFX *gfx = new Arduino_ST7789(bus, PIN_RST, 1, true, 172, 320, 34, 0, 34, 0);
I2SClass i2s;
int cur = 0;
bool locked = false, lastBtn = HIGH;
int16_t buf[256];

void show() {
  const Combo &c = combos[cur];
  gfx->fillScreen(0x0000);
  gfx->setTextSize(2);
  gfx->setTextColor(locked ? 0x07E0 : 0xFFE0);
  gfx->setCursor(10, 10);
  gfx->printf(locked ? "LOCKED %d/%d" : "AUDIO TEST %d/%d", cur + 1, N);
  gfx->setTextSize(3);
  gfx->setTextColor(0xFFFF);
  gfx->setCursor(10, 50);  gfx->printf("BCLK = %d", c.bclk);
  gfx->setCursor(10, 85);  gfx->printf("LRC  = %d", c.lrc);
  gfx->setCursor(10, 120); gfx->printf("DIN  = %d", c.din);
  gfx->setTextSize(1);
  gfx->setTextColor(0x8410);
  gfx->setCursor(10, 158);
  gfx->print(locked ? "BOOT: continue scanning" : "Hear beeps? press BOOT");
  Serial.printf("Combo %d: BCLK=%d LRC=%d DIN=%d %s\n", cur + 1, c.bclk, c.lrc, c.din, locked ? "(locked)" : "");
}

void startI2S() {
  i2s.end();
  // release all candidate pins before reassigning them
  for (int p : {4, 5, 16, 17}) pinMode(p, INPUT);
  const Combo &c = combos[cur];
  i2s.setPins(c.bclk, c.lrc, c.din);
  if (!i2s.begin(I2S_MODE_STD, RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO))
    Serial.println("I2S begin failed");
}

bool checkButton() {
  bool b = digitalRead(PIN_BTN), pressed = (b == LOW && lastBtn == HIGH);
  lastBtn = b;
  if (pressed) { locked = !locked; show(); }
  return pressed;
}

// Beep pattern: the number of beeps = combo number, so you can tell them apart
void beepPattern() {
  float phase = 0;
  for (int k = 0; k <= cur; k++) {
    for (int i = 0; i < RATE / 5; i += 128) {     // 200ms beep
      for (int j = 0; j < 128; j++) {
        phase += 700.0f / RATE; if (phase >= 1) phase -= 1;
        int16_t v = sinf(2 * PI * phase) * VOLUME * 32767;
        buf[j * 2] = v; buf[j * 2 + 1] = v;
      }
      i2s.write((uint8_t *)buf, sizeof(buf));
      checkButton();
    }
    memset(buf, 0, sizeof(buf));
    for (int i = 0; i < RATE / 6; i += 128) { i2s.write((uint8_t *)buf, sizeof(buf)); checkButton(); }
  }
  memset(buf, 0, sizeof(buf));
  for (int i = 0; i < RATE; i += 128) { i2s.write((uint8_t *)buf, sizeof(buf)); checkButton(); }  // 1s gap
}

void setup() {
  Serial.begin(115200);
  analogWrite(PIN_BL, 255);
  pinMode(PIN_BTN, INPUT_PULLUP);
  gfx->begin(40000000);
  startI2S();
  show();
}

void loop() {
  unsigned long start = millis();
  while (millis() - start < 6000 || locked) beepPattern();   // ~6s per combination
  cur = (cur + 1) % N;
  startI2S();
  show();
}
