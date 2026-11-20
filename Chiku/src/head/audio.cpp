// audio.cpp - see audio.h
#include "audio.h"
#include "config.h"
#include "logbuf.h"
#include "bus.h"
#include "storage.h"
#include <ESP_I2S.h>
#include <LittleFS.h>
#include <math.h>

namespace audio {

static const uint32_t RATE = 16000;
static I2SClass i2s;
static bool ok = false;
static QueueHandle_t noteQ = nullptr;
static volatile bool stopFlag = false;
static volatile bool loopFlag = false;
static volatile Melody loopMelody = MEL_ALARM;
static volatile bool busy = false;
static volatile uint8_t volume = 60;

struct Note { uint16_t hz; uint16_t ms; uint16_t hz2 = 0; uint8_t voice = 0; };   // hz2 != 0: glide hz -> hz2

// Note frequencies
#define C5 523
#define D5 587
#define E5 659
#define F5 698
#define G5 784
#define A5 880
#define B5 988
#define C6 1047
#define E6 1319
#define G6 1568
#define R 0

static const Note M_STARTUP[] = {{C5, 90}, {E5, 90}, {G5, 90}, {C6, 180}};
static const Note M_ALARM[]   = {{A5, 120}, {R, 60}, {A5, 120}, {R, 60}, {C6, 120}, {R, 60}, {C6, 120}, {R, 400}};
static const Note M_HAPPY[]   = {{E5, 80}, {G5, 80}, {C6, 80}, {E6, 160}};
static const Note M_SAD[]     = {{G5, 200}, {E5, 200}, {C5, 400}};
static const Note M_PET[]     = {{G5, 70}, {B5, 70}, {G6, 140}};
static const Note M_TIMER[]   = {{C6, 100}, {R, 60}, {C6, 100}, {R, 60}, {C6, 100}, {R, 300}};
static const Note M_DIZZY[]   = {{E5, 70}, {G5, 70}, {E5, 70}, {C5, 70}, {E5, 70}, {G5, 70}, {C5, 150}};
static const Note M_CLICK[]   = {{C6, 25}};

struct Tune { const Note* n; uint8_t count; };
#define TUNE(a) {a, (uint8_t)(sizeof(a) / sizeof(a[0]))}
static const Tune TUNES[MEL_COUNT] = {TUNE(M_STARTUP), TUNE(M_ALARM), TUNE(M_HAPPY), TUNE(M_SAD),
                                      TUNE(M_PET), TUNE(M_TIMER), TUNE(M_DIZZY), TUNE(M_CLICK)};

// ---- ES8311 codec setup (register sequence from the Waveshare ES8311 driver) ----
static uint8_t rd(uint8_t reg) { uint8_t v = 0; bus::readRegs(I2C_ADDR_ES8311, reg, &v, 1); return v; }
static void wr(uint8_t reg, uint8_t v) { bus::writeReg(I2C_ADDR_ES8311, reg, v); }

static void setCodecVolume(uint8_t pct) {
  uint8_t reg = pct == 0 ? 0 : (uint8_t)((pct * 256) / 100 - 1);
  wr(0x32, reg);
}

static bool codecInit() {
  if (!bus::probe(I2C_ADDR_ES8311)) return false;
  wr(0x00, 0x1F); delay(20);
  wr(0x00, 0x00);
  wr(0x00, 0x80);                               // power on
  wr(0x01, 0x3F);                               // all clocks, MCLK from pin
  wr(0x06, rd(0x06) & ~0x20);                   // SCLK not inverted
  // 16 kHz with MCLK = 256 * fs = 4.096 MHz
  wr(0x02, (rd(0x02) & 0x07) | (0 << 5) | (0 << 3));
  wr(0x03, 0x10);
  wr(0x04, 0x10);
  wr(0x05, 0x00);
  wr(0x06, (rd(0x06) & 0xE0) | (4 - 1));
  wr(0x07, (rd(0x07) & 0xC0) | 0x00);
  wr(0x08, 0xFF);
  wr(0x00, rd(0x00) & 0xBF);                    // slave mode
  wr(0x09, 0x0C);                               // 16 bit I2S in
  wr(0x0A, 0x0C);                               // 16 bit I2S out
  wr(0x0D, 0x01);                               // power up analog
  wr(0x0E, 0x02);
  wr(0x12, 0x00);                               // power up DAC
  wr(0x13, 0x10);                               // enable output
  wr(0x1C, 0x6A);
  wr(0x37, 0x08);                               // bypass DAC equaliser
  setCodecVolume(85);
  return true;
}

static volatile bool mutedFlag = false;
void setMuted(bool on) { LOGI("audio", "mute %s", on ? "on" : "off"); mutedFlag = on; if (on) stop(); }
bool muted() { return mutedFlag; }
void setVolume(uint8_t p) { volume = constrain(p, 0, 100); }
bool online() { return ok; }
bool playing() { return busy || (noteQ && uxQueueMessagesWaiting(noteQ) > 0); }

void beep(uint16_t hz, uint16_t ms) {
  if (!ok || mutedFlag) return;
  Note n = {hz, ms};
  xQueueSend(noteQ, &n, 0);
}

void play(Melody m, bool loop) {
  if (!ok || mutedFlag || m >= MEL_COUNT) return;
  loopFlag = loop;
  loopMelody = m;
  for (uint8_t i = 0; i < TUNES[m].count; i++) xQueueSend(noteQ, &TUNES[m].n[i], 0);
}

void stop() {
  loopFlag = false;
  stopFlag = true;
}

// Make one note: sine with a short fade in/out so it does not click.
static void playNote(const Note& n) {
  static int16_t buf[256 * 2];
  uint32_t total = (uint32_t)RATE * n.ms / 1000;
  float phase = 0, inc = 2.f * PI * n.hz / RATE;
  float incEnd = n.hz2 ? 2.f * PI * n.hz2 / RATE : inc;
  float amp = 14000.f * (volume / 100.f) * (volume / 100.f);
  uint32_t done = 0;
  while (done < total && !stopFlag) {
    uint32_t chunk = min<uint32_t>(256, total - done);
    for (uint32_t i = 0; i < chunk; i++) {
      float env = 1.f;
      uint32_t pos = done + i, fade = RATE / 200;      // 5 ms
      if (pos < fade) env = (float)pos / fade;
      else if (total - pos < fade) env = (float)(total - pos) / fade;
      float wave = sinf(phase);
      if (n.voice) wave = 0.65f * wave + 0.25f * sinf(2 * phase) + 0.10f * sinf(3 * phase);   // richer, more 'voice' like
      int16_t s = n.hz ? (int16_t)(wave * amp * env) : 0;
      inc += (incEnd - inc) * (3.f / total);          // glide towards the end pitch
      phase += inc;
      if (phase > 2.f * PI) phase -= 2.f * PI;
      buf[2 * i] = s; buf[2 * i + 1] = s;
    }
    i2s.write((uint8_t*)buf, chunk * 4);
    done += chunk;
  }
}

// ---- babble ("cute talk") ----
void babble(Babble style, uint8_t n) {
  if (!ok || mutedFlag) return;
  n = constrain(n, 1, 6);
  static const uint16_t BASE[BAB_COUNT] = {520, 420, 190, 480, 600, 450, 400};
  float base = BASE[style];
  for (uint8_t i = 0; i < n; i++) {
    float step = (i % 2) ? 1.18f : 0.92f;                   // alternate high / low like "chi-ku"
    float f = base * step * (1.f + 0.06f * i);
    Note s = {(uint16_t)f, 95, (uint16_t)f, 1};
    switch (style) {
      case BAB_HAPPY:    s.hz2 = f * 1.35f; break;           // each syllable swoops up
      case BAB_SAD:      s.hz2 = f * 0.70f; s.ms = 150; f *= 0.9f; break;
      case BAB_ANGRY:    s.hz2 = f * 0.85f; s.ms = 70; break;
      case BAB_SURPRISE: s.hz2 = f * 1.8f; s.ms = 80; break;
      case BAB_SHY:      s.hz2 = f * 1.1f; s.ms = 85; break;
      case BAB_LOVE:     s.hz2 = f * 1.2f; s.ms = 170; break;
      case BAB_QUESTION: s.hz2 = (i == n - 1) ? f * 1.9f : f; s.ms = (i == n - 1) ? 220 : 95; break;
      default: break;
    }
    s.hz = (uint16_t)f;
    xQueueSend(noteQ, &s, 0);
    Note gap = {0, 35};
    xQueueSend(noteQ, &gap, 0);
  }
}

// ---- WAV clips ----
static char wavPath[48] = "";
static volatile bool wavReq = false;

bool hasClip(const char* key) {
  char p[48];
  snprintf(p, sizeof(p), "/voice/%s.wav", key);
  return storage::fsReady() && LittleFS.exists(p);
}

bool playWav(const char* path) {
  if (!ok || mutedFlag || !storage::fsReady() || !LittleFS.exists(path)) return false;
  strlcpy(wavPath, path, sizeof(wavPath));
  wavReq = true;
  return true;
}

void say(const char* key, Babble fallback) {
  char p[48];
  snprintf(p, sizeof(p), "/voice/%s.wav", key);
  if (playWav(p)) LOGI("audio", "say clip %s", key);
  else { LOGW("audio", "say %s: no clip (or muted) -> babble", key); babble(fallback); }
}

// Play a canonical 44-byte-header PCM16 WAV (mono or stereo, 16 kHz).
static void playWavFile(const char* path) {
  File f = LittleFS.open(path, "r");
  if (!f || f.size() < 48) return;
  uint8_t hdr[44];
  f.read(hdr, 44);
  uint16_t channels = hdr[22] | (hdr[23] << 8);
  uint16_t bits = hdr[34] | (hdr[35] << 8);
  if (memcmp(hdr, "RIFF", 4) || bits != 16 || channels < 1 || channels > 2) { f.close(); return; }
  static int16_t in[512], out[1024];
  float gain = (volume / 100.f) * (volume / 100.f) * 1.6f;
  while (!stopFlag) {
    int n = f.read((uint8_t*)in, sizeof(in)) / 2;           // samples
    if (n <= 0) break;
    int frames = n / channels;
    for (int i = 0; i < frames; i++) {
      int16_t l = (int16_t)constrain(in[i * channels] * gain, -32768.f, 32767.f);
      int16_t r = channels == 2 ? (int16_t)constrain(in[i * 2 + 1] * gain, -32768.f, 32767.f) : l;
      out[2 * i] = l; out[2 * i + 1] = r;
    }
    i2s.write((uint8_t*)out, frames * 4);
  }
  f.close();
}

static void audioTask(void*) {
  Note n;
  uint32_t silentSince = millis();
  bool paOn = false;
  for (;;) {
    if (stopFlag) { xQueueReset(noteQ); stopFlag = false; wavReq = false; }
    if (wavReq) {                                         // play a voice clip
      wavReq = false;
      if (!paOn) { digitalWrite(PIN_PA_CTRL, HIGH); paOn = true; delay(20); }
      busy = true;
      playWavFile(wavPath);
      silentSince = millis();
    }
    if (xQueueReceive(noteQ, &n, pdMS_TO_TICKS(20)) == pdTRUE) {
      if (!paOn) { digitalWrite(PIN_PA_CTRL, HIGH); paOn = true; delay(20); }
      busy = true;
      playNote(n);
      if (loopFlag && uxQueueMessagesWaiting(noteQ) == 0) {   // repeat the melody
        const Tune& t = TUNES[loopMelody];
        for (uint8_t i = 0; i < t.count; i++) xQueueSend(noteQ, &t.n[i], 0);
      }
      silentSince = millis();
    } else {
      busy = false;
      if (paOn && millis() - silentSince > 200) { digitalWrite(PIN_PA_CTRL, LOW); paOn = false; }
    }
  }
}

bool init() {
  pinMode(PIN_PA_CTRL, OUTPUT);
  digitalWrite(PIN_PA_CTRL, LOW);
  volume = storage::get().volume;
  i2s.setPins(PIN_I2S_BCLK, PIN_I2S_WS, PIN_I2S_DOUT, PIN_I2S_DIN, PIN_I2S_MCLK);
  if (!i2s.begin(I2S_MODE_STD, RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO, I2S_STD_SLOT_BOTH)) return false;
  if (!codecInit()) return false;
  noteQ = xQueueCreate(48, sizeof(Note));
  ok = true;
  xTaskCreatePinnedToCore(audioTask, "audio", 4096, nullptr, 2, nullptr, 0);
  return true;
}

}  // namespace audio
