// Native tests for the joystick mapping.
#include "../src/mapping.h"
#include <iostream>
using namespace mapping;
static int failed = 0;
#define CHECK(name, cond) do { bool ok_ = (cond); std::cout << (ok_ ? "PASS  " : "FAIL  ") << name << "\n"; if (!ok_) failed++; } while (0)
int main() {
  CHECK("centre -> 0", normalize(2048, 300, 2048, 3800) == 0.f);
  CHECK("max -> +1", fabsf(normalize(3800, 300, 2048, 3800) - 1.f) < 1e-6);
  CHECK("min -> -1", fabsf(normalize(300, 300, 2048, 3800) + 1.f) < 1e-6);
  CHECK("beyond the calibrated range is clamped", normalize(4095, 300, 2048, 3800) == 1.f && normalize(0, 300, 2048, 3800) == -1.f);
  CHECK("asymmetric travel still reaches +-1", fabsf(normalize(2048 + 900, 300, 2048, 2948) - 1.f) < 1e-6 && fabsf(normalize(300, 300, 2048, 2948) + 1.f) < 1e-6);
  CHECK("dead zone gives exactly 0", shape(0.05f, 0.07f, 0.4f, 1.f) == 0.f && shape(-0.07f, 0.07f, 0.4f, 1.f) == 0.f);
  CHECK("just outside the dead zone starts near 0", shape(0.08f, 0.07f, 0.4f, 1.f) < 0.01f && shape(0.08f, 0.07f, 0.4f, 1.f) > 0.f);
  CHECK("full deflection is exactly 1", fabsf(shape(1.f, 0.07f, 0.4f, 1.f) - 1.f) < 1e-6 && fabsf(shape(-1.f, 0.07f, 0.4f, 1.f) + 1.f) < 1e-6);
  CHECK("expo is softer than linear in the middle", shape(0.5f, 0.f, 0.5f, 1.f) < 0.5f);
  CHECK("output is monotonic", [] { float p = -1; for (float v = -1; v <= 1; v += 0.01f) { float o = shape(v, 0.07f, 0.45f, 1.f); if (o < p) return false; p = o; } return true; }());
  CHECK("fine mode scales down", fabsf(shape(1.f, 0.07f, 0.4f, 0.45f) - 0.45f) < 1e-6);
  CHECK("toI8 limits", toI8(1.f) == 127 && toI8(-1.f) == -127 && toI8(0.f) == 0 && toI8(5.f) == 127 && toI8(-5.f) == -127);
  std::cout << (failed ? "\nFAILED\n" : "\nall passed\n");
  return failed ? 1 : 0;
}
