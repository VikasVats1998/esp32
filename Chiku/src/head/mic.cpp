// mic.cpp - see mic.h. ES7210 register setup follows Espressif's es7210 driver (esp_codec_dev):
// I2S slave, 16 bit, 16 kHz with MCLK = 256 * fs, microphones 1+2 on, 3+4 off.
#include "mic.h"
#include "config.h"
#include "logbuf.h"
#include "bus.h"
#include "events.h"
#include "audio.h"
#include <math.h>

namespace mic {

static bool ok = false;
static volatile float lvl = 0;
static volatile int dbVal = -1;
static volatile uint32_t burstCount = 0;

static void wr(uint8_t reg, uint8_t v) { bus::writeReg(I2C_ADDR_ES7210, reg, v); }

static bool codecInit() {
  if (!bus::probe(I2C_ADDR_ES7210)) return false;
  wr(0x00, 0xFF); delay(10);              // reset
  wr(0x00, 0x41);
  wr(0x01, 0x3F);                         // clocks off while configuring
  wr(0x09, 0x30); wr(0x0A, 0x30);         // power-up timing
  wr(0x23, 0x2A); wr(0x22, 0x0A);         // high-pass filters
  wr(0x20, 0x0A); wr(0x21, 0x2A);
  wr(0x08, 0x20);                         // slave mode
  wr(0x40, 0x43);                         // analog reference
  wr(0x41, 0x70); wr(0x42, 0x70);         // mic bias 2.87 V
  wr(0x07, 0x20);                         // OSR
  wr(0x02, 0xC1);                         // MCLK 4.096 MHz -> 16 kHz (div 1, doubler, DLL)
  wr(0x04, 0x01); wr(0x05, 0x00);         // LRCK divider 256
  wr(0x11, 0x60);                         // I2S format, 16 bit
  wr(0x12, 0x00);                         // normal (no TDM): mic1 left, mic2 right
  wr(0x43, 0x10 | MIC_GAIN); wr(0x44, 0x10 | MIC_GAIN);   // mic 1/2 on, gain
  wr(0x45, 0x00); wr(0x46, 0x00);         // mic 3/4 off
  wr(0x47, 0x08); wr(0x48, 0x08);
  wr(0x4B, 0x00);                         // mic 1/2 power on
  wr(0x4C, 0xFF);                         // mic 3/4 power off
  wr(0x01, 0x14);                         // clocks on for ADC 1/2
  wr(0x06, 0x00);                         // power on
  wr(0x00, 0x71); wr(0x00, 0x41);         // start
  return true;
}

static void micTask(void*) {
  static int16_t buf[256 * 2];
  uint32_t loudSince = 0, quietSince = millis(), lastLoudEvt = 0;
  for (;;) {
    size_t n = audio::micRead(buf, 256);
    if (n == 0) { vTaskDelay(pdMS_TO_TICKS(50)); continue; }
    float sum = 0;
    for (size_t i = 0; i < n; i++) sum += (float)buf[2 * i] * buf[2 * i];
    float rms = sqrtf(sum / n) + 1.f;
    float dbfs = 20.f * log10f(rms / 32768.f);
    dbVal = (int)(dbfs + MIC_DB_OFFSET);
    float target = constrain((dbfs + 60.f) / 45.f, 0.f, 1.f);
    lvl = lvl + (target - lvl) * (target > lvl ? 0.6f : 0.15f);   // fast attack, slow release
    uint32_t now = millis();
    bool selfSound = audio::playing();
    bool loud = dbVal >= MIC_VOICE_DB && !selfSound;
    if (loud) {
      if (!loudSince) loudSince = now;
      if (dbVal >= MIC_LOUD_DB && now - quietSince < 300 && now - lastLoudEvt > 3000) {   // sudden bang
        lastLoudEvt = now;
        events::publish(events::EV_LOUD, dbVal);
      }
    } else {
      if (loudSince && now - loudSince >= 250 && now - loudSince <= 3000) burstCount = burstCount + 1;
      loudSince = 0;
      quietSince = now;
    }
  }
}

bool init() {
  if (!audio::online()) return false;
  bus::lock();
  ok = codecInit();
  bus::unlock();
  if (!ok) { LOGW("mic", "ES7210 not found at 0x%02X", I2C_ADDR_ES7210); return false; }
  xTaskCreatePinnedToCore(micTask, "mic", 3072, nullptr, 2, nullptr, 0);
  LOGI("mic", "ES7210 ready");
  return true;
}

bool online() { return ok; }
float level() { return lvl; }
int db() { return ok ? dbVal : -1; }
uint32_t bursts() { return burstCount; }

}  // namespace mic
