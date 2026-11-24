// mic.cpp - see mic.h
#include "mic.h"
#include "config.h"
#include "logbuf.h"
#include "events.h"
#include "audio.h"
#include "storage.h"
#include <math.h>

namespace mic {

static volatile int lvl = 0, pk = 0;
static volatile float floorLvl = 0;
static bool ok = false;

bool online() { return ok; }
int level() { return lvl; }
int peak() { return pk; }
float noiseFloor() { return floorLvl; }
static volatile int dbVal = -1;
static volatile uint32_t burstCount = 0;
int db() { return ok ? dbVal : -1; }
uint32_t bursts() { return burstCount; }

static int toPct(float amp) {                    // amplitude 0..32767 -> 0..100 (-60..-10 dBFS)
  if (amp < 1) return 0;
  float db = 20.f * log10f(amp / 32768.f);
  return constrain((int)((db + 60.f) * 2.f), 0, 100);
}

static volatile bool external = false;           // the speech engine reads the mic, we are fed by the tap
static int16_t blockBuf[256 * 2];

// state of the analyser (shared by the task and the tap)
static float smooth = 0, floorAmp = 300, prevPeak = 0;
static uint32_t lastClap = 0, firstClap = 0, quietUntil = 0;
static uint8_t claps = 0;

// Analyse interleaved stereo frames (any block length).
static void feed(const int16_t* buf, size_t n) {
  float sum = 0, mx = 0;
  for (size_t i = 0; i < n; i++) {
    float m = 0.5f * (buf[2 * i] + buf[2 * i + 1]);
    sum += m * m;
    float a = fabsf(m);
    if (a > mx) mx = a;
  }
  float rms = sqrtf(sum / (n ? n : 1));
  smooth += (rms - smooth) * (rms > smooth ? 0.5f : 0.1f);
  lvl = toPct(smooth);
  pk = toPct(mx);
  uint32_t now = millis();
  if (audio::playing()) quietUntil = now + 400;
  // UI kit: dB value, sudden loud sound (EV_LOUD), voice bursts for the setup mic check
  dbVal = (int)(20.f * log10f((smooth + 1.f) / 32768.f) + MIC_DB_OFFSET);
  static uint32_t loudSince = 0, calmSince = 0, lastLoudEvt = 0;
  if (now > quietUntil && dbVal >= MIC_VOICE_DB) {
    if (!loudSince) loudSince = now;
    if (dbVal >= MIC_LOUD_DB && now - calmSince < 300 && now - lastLoudEvt > 3000) {
      lastLoudEvt = now;
      events::publish(events::EV_LOUD, dbVal);
    }
  } else {
    if (loudSince && now - loudSince >= 250 && now - loudSince <= 3000) burstCount = burstCount + 1;
    loudSince = 0;
    calmSince = now;
  }
  if (rms < floorAmp * 2.5f) floorAmp += (rms - floorAmp) * 0.01f;
  floorLvl = toPct(floorAmp);
  Settings& s = storage::get();
  if (s.clapWake && now > quietUntil) {
    float thr = fmaxf(3000.f + (100 - s.clapSens) * 110.f, floorAmp * 8.f);
    bool clap = mx > thr && prevPeak < thr * 0.5f && now - lastClap > 120;
    if (clap) {
      lastClap = now;
      if (!claps) { claps = 1; firstClap = now; }
      else if (now - firstClap < 900) { claps = 0; events::publish(events::EV_CLAP, 2); LOGI("mic", "double clap (peak %d)", (int)mx); }
    }
    if (claps == 1 && now - firstClap > 900) { claps = 0; events::publish(events::EV_CLAP, 1); LOGI("mic", "clap (peak %d)", (int)mx); }
  } else claps = 0;
  prevPeak = mx;
}

void useExternalReader(bool on) {
  external = on;
  audio::micTap(on ? feed : nullptr);
}

static void task(void*) {
  for (;;) {
    if (external) { vTaskDelay(pdMS_TO_TICKS(50)); continue; }
    size_t got = audio::micRead((int16_t*)blockBuf, sizeof(blockBuf));
    if (!got) { vTaskDelay(pdMS_TO_TICKS(5)); continue; }
    feed(blockBuf, got / 4);
  }
}

void init() {
  ok = audio::micBegin();
  LOGI("mic", "microphone %s", ok ? "ready" : "NOT available");
  if (ok) xTaskCreatePinnedToCore(task, "mic", 4096, nullptr, 1, nullptr, 0);
}

}  // namespace mic
