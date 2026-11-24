// mapping.h - joystick shaping (pure functions, tested on a PC: test/run.sh).
#pragma once
#include <math.h>
#include <stdint.h>

namespace mapping {

// Raw ADC value -> -1..1 using the calibrated minimum, centre and maximum.
inline float normalize(int raw, int minV, int center, int maxV) {
  float v;
  if (raw >= center) v = (maxV > center) ? (float)(raw - center) / (maxV - center) : 0.f;
  else v = (center > minV) ? (float)(raw - center) / (center - minV) : 0.f;
  return v > 1.f ? 1.f : v < -1.f ? -1.f : v;
}

// Dead zone (rescaled so the output starts at 0 right after it) and an expo curve (soft near the centre, full at the ends).
inline float shape(float v, float deadzone, float expo, float scale) {
  float a = fabsf(v);
  if (a <= deadzone) return 0.f;
  a = (a - deadzone) / (1.f - deadzone);
  a = (1.f - expo) * a + expo * a * a * a;
  a *= scale;
  return v < 0 ? -a : a;
}

inline int8_t toI8(float v) {
  int i = (int)lroundf(v * 127.f);
  return (int8_t)(i > 127 ? 127 : i < -127 ? -127 : i);
}

}  // namespace mapping
