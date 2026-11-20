// bus.cpp - see bus.h
#include "bus.h"
#include "config.h"

namespace bus {

static SemaphoreHandle_t mtx = nullptr;

void init() {
  if (mtx) return;
  mtx = xSemaphoreCreateRecursiveMutex();
  Wire.begin(PIN_I2C0_SDA, PIN_I2C0_SCL, 400000);
  Wire.setTimeOut(20);
}

void lock() { xSemaphoreTakeRecursive(mtx, portMAX_DELAY); }
void unlock() { xSemaphoreGiveRecursive(mtx); }

bool writeRegs(uint8_t addr, uint8_t reg, const uint8_t* data, size_t len) {
  Guard g;
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (len) Wire.write(data, len);
  return Wire.endTransmission() == 0;
}

bool writeReg(uint8_t addr, uint8_t reg, uint8_t val) { return writeRegs(addr, reg, &val, 1); }

bool readRegs(uint8_t addr, uint8_t reg, uint8_t* data, size_t len) {
  Guard g;
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  size_t got = Wire.requestFrom((int)addr, (int)len);
  if (got != len) return false;
  for (size_t i = 0; i < len; i++) data[i] = Wire.read();
  return true;
}

bool probe(uint8_t addr) {
  Guard g;
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

}  // namespace bus
