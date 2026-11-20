// bus.h - the onboard I2C bus (GPIO14/15) shared by touch, IMU, RTC, PMU and
// the audio codec. Many tasks use it, so every access takes a mutex.
// Also holds the tiny register helpers used by simple drivers.
#pragma once
#include <Arduino.h>
#include <Wire.h>

namespace bus {
void init();
void lock();
void unlock();
bool writeReg(uint8_t addr, uint8_t reg, uint8_t val);
bool writeRegs(uint8_t addr, uint8_t reg, const uint8_t* data, size_t len);
bool readRegs(uint8_t addr, uint8_t reg, uint8_t* data, size_t len);
bool probe(uint8_t addr);

// Scope guard: lock in constructor, unlock in destructor.
struct Guard {
  Guard() { lock(); }
  ~Guard() { unlock(); }
};
}  // namespace bus
