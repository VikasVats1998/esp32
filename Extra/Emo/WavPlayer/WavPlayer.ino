// Plays an embedded PCM WAV through a MAX98357A I2S amplifier.
// The display is not used; playback status is printed to Serial.

#include <Arduino.h>
#include <ESP_I2S.h>

#include "board_config.h"
#include "audio_data.h"

namespace {

constexpr uint16_t kFramesPerBuffer = 128;
constexpr uint32_t kMinSampleRate = 8000;
constexpr uint32_t kMaxSampleRate = 96000;

struct WavInfo {
  uint32_t sampleRate;
  uint16_t channels;
  uint16_t bitsPerSample;
  uint16_t blockAlign;
  const uint8_t *data;
  uint32_t dataSize;
};

I2SClass i2s;
int16_t outputBuffer[kFramesPerBuffer * 2];

uint16_t read16(const uint8_t *p) {
  return static_cast<uint16_t>(p[0]) |
         (static_cast<uint16_t>(p[1]) << 8);
}

uint32_t read32(const uint8_t *p) {
  return static_cast<uint32_t>(p[0]) |
         (static_cast<uint32_t>(p[1]) << 8) |
         (static_cast<uint32_t>(p[2]) << 16) |
         (static_cast<uint32_t>(p[3]) << 24);
}

bool hasId(const uint8_t *p, const char *id) {
  return p[0] == id[0] && p[1] == id[1] &&
         p[2] == id[2] && p[3] == id[3];
}

bool parseWav(WavInfo &wav) {
  if (audioDataSize < 12 || !hasId(audioData, "RIFF") ||
      !hasId(audioData + 8, "WAVE")) {
    Serial.println("Invalid WAV: expected a RIFF/WAVE file.");
    return false;
  }

  bool haveFormat = false;
  bool haveData = false;
  uint16_t format = 0;
  uint32_t offset = 12;

  while (offset <= audioDataSize && audioDataSize - offset >= 8) {
    const uint8_t *chunk = audioData + offset;
    const uint32_t chunkSize = read32(chunk + 4);
    const uint32_t body = offset + 8;
    if (chunkSize > audioDataSize - body) {
      Serial.println("Invalid WAV: chunk extends beyond embedded data.");
      return false;
    }

    if (hasId(chunk, "fmt ")) {
      if (chunkSize < 16) {
        Serial.println("Invalid WAV: format chunk is too short.");
        return false;
      }
      format = read16(audioData + body);
      wav.channels = read16(audioData + body + 2);
      wav.sampleRate = read32(audioData + body + 4);
      wav.blockAlign = read16(audioData + body + 12);
      wav.bitsPerSample = read16(audioData + body + 14);
      haveFormat = true;
    } else if (hasId(chunk, "data")) {
      wav.data = audioData + body;
      wav.dataSize = chunkSize;
      haveData = true;
    }

    const uint32_t paddedSize = chunkSize + (chunkSize & 1U);
    if (paddedSize > audioDataSize - body) break;
    offset = body + paddedSize;
  }

  if (!haveFormat || !haveData) {
    Serial.println("Invalid WAV: missing format or audio data chunk.");
    return false;
  }
  if (format != 1) {
    Serial.println("Unsupported WAV: only integer PCM is supported.");
    return false;
  }
  if (wav.channels != 1 && wav.channels != 2) {
    Serial.println("Unsupported WAV: use mono or stereo audio.");
    return false;
  }
  if (wav.bitsPerSample != 8 && wav.bitsPerSample != 16 &&
      wav.bitsPerSample != 24 && wav.bitsPerSample != 32) {
    Serial.println("Unsupported WAV: use 8-, 16-, 24-, or 32-bit PCM.");
    return false;
  }
  if (wav.sampleRate < kMinSampleRate || wav.sampleRate > kMaxSampleRate) {
    Serial.println("Unsupported WAV sample rate (use 8000-96000 Hz).");
    return false;
  }

  const uint16_t bytesPerSample = wav.bitsPerSample / 8;
  if (wav.blockAlign != bytesPerSample * wav.channels || wav.blockAlign == 0) {
    Serial.println("Invalid WAV: inconsistent block alignment.");
    return false;
  }
  return true;
}

int16_t readSample(const uint8_t *p, uint16_t bits) {
  if (bits == 8) {
    return static_cast<int16_t>((static_cast<int16_t>(p[0]) - 128) << 8);
  }
  if (bits == 16) {
    return static_cast<int16_t>(read16(p));
  }
  if (bits == 24) {
    int32_t sample = static_cast<int32_t>(p[0]) |
                     (static_cast<int32_t>(p[1]) << 8) |
                     (static_cast<int32_t>(p[2]) << 16);
    if (sample & 0x00800000) sample |= static_cast<int32_t>(0xFF000000);
    return static_cast<int16_t>(sample >> 8);
  }
  const uint32_t raw = read32(p);
  const int64_t signedSample =
      raw <= 0x7FFFFFFFUL
          ? static_cast<int64_t>(raw)
          : static_cast<int64_t>(raw) - 0x100000000LL;
  return static_cast<int16_t>(signedSample >> 16);
}

int16_t applyVolume(int32_t sample) {
  const int32_t scaled =
      static_cast<int32_t>(static_cast<float>(sample) * WAVPLAYER_VOLUME);
  if (scaled > 32767) return 32767;
  if (scaled < -32768) return -32768;
  return static_cast<int16_t>(scaled);
}

bool playWav(const WavInfo &wav) {
  const uint32_t bytesPerSample = wav.bitsPerSample / 8;
  const uint32_t frameCount = wav.dataSize / wav.blockAlign;

  i2s.setPins(WAVPLAYER_I2S_BCLK, WAVPLAYER_I2S_LRC, WAVPLAYER_I2S_DIN);
  if (!i2s.begin(I2S_MODE_STD, wav.sampleRate, I2S_DATA_BIT_WIDTH_16BIT,
                 I2S_SLOT_MODE_STEREO)) {
    Serial.println("I2S initialization failed.");
    return false;
  }

  Serial.printf("Playing %.2f seconds at %lu Hz\n",
                static_cast<double>(frameCount) / wav.sampleRate,
                static_cast<unsigned long>(wav.sampleRate));

  for (uint32_t frame = 0; frame < frameCount;) {
    const uint16_t count = min(
        static_cast<uint32_t>(kFramesPerBuffer), frameCount - frame);
    const uint8_t *source = wav.data + frame * wav.blockAlign;

    for (uint16_t i = 0; i < count; ++i) {
      const uint8_t *left = source + i * wav.blockAlign;
      int32_t sample = readSample(left, wav.bitsPerSample);
      if (wav.channels == 2) {
        const int16_t right = readSample(left + bytesPerSample, wav.bitsPerSample);
        sample = (sample + right) / 2;
      }
      const int16_t output = applyVolume(sample);
      outputBuffer[i * 2] = output;
      outputBuffer[i * 2 + 1] = output;
    }

    const size_t byteCount = static_cast<size_t>(count) * 2 * sizeof(int16_t);
    const size_t written = i2s.write(
        reinterpret_cast<uint8_t *>(outputBuffer), byteCount);
    if (written != byteCount) {
      Serial.printf("I2S write incomplete (%u of %u bytes).\n",
                    static_cast<unsigned>(written),
                    static_cast<unsigned>(byteCount));
      i2s.end();
      return false;
    }
    frame += count;
  }

  memset(outputBuffer, 0, sizeof(outputBuffer));
  for (uint8_t i = 0; i < 4; ++i) {
    i2s.write(reinterpret_cast<uint8_t *>(outputBuffer), sizeof(outputBuffer));
  }
  i2s.end();
  Serial.println("Playback finished.");
  return true;
}

void playEmbeddedWav() {
  WavInfo wav = {};
  if (audioDataSize == 0) {
    Serial.println("No audio embedded. Generate WavPlayer/audio_data.h first.");
    return;
  }
  if (parseWav(wav)) playWav(wav);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  pinMode(WAVPLAYER_BUTTON, INPUT_PULLUP);
  delay(300);
  Serial.println("\nWAV player ready.");
  playEmbeddedWav();
}

void loop() {
  static bool previousButton = HIGH;
  const bool button = digitalRead(WAVPLAYER_BUTTON);
  if (button == LOW && previousButton == HIGH) {
    delay(25);
    if (digitalRead(WAVPLAYER_BUTTON) == LOW) playEmbeddedWav();
  }
  previousButton = button;
  delay(10);
}
