// imu.cpp - see imu.h
#include "imu.h"
#include "config.h"
#include "bus.h"
#include "events.h"
#include <SensorQMI8658.hpp>
#include <math.h>

namespace imu {

static SensorQMI8658 qmi;
static bool ok = false;
static float scaleToG = 1.f;
static float ax = 0, ay = 0, az = 1;
static float refX = 0, refY = 0, refZ = 1;      // upright gravity direction
static float tilt = 0;
static float rawGx = 0, rawGy = 0, rawGz = 0;
static float gx = 0, gy = 0, gz = 0;            // gyro, deg/s (bias removed)
static float gbias[3] = {0, 0, 0};              // gyro zero offset, learned at boot while still
static bool gyroCal = false;
static volatile bool recalReq = false;
static float fRoll = 0, fPitch = 0;             // complementary filter output
static bool flip = false;
static uint32_t flipSince = 0, lastUs = 0;

// shake detector
static uint32_t swingTimes[6];
static uint8_t swingCount = 0;
static float lastMag = 1;
static uint32_t shakeCooldown = 0, fallCooldown = 0, tapCooldown = 0;
// state
static uint32_t freefallSince = 0, tiltSince = 0, liftSince = 0, stillSince = 0;
static bool pickedUp = false;
static float lastRoll = 0, lastPitch = 0;
static uint32_t lastTiltEvt = 0;

bool online() { return ok; }
void getAccel(float& x, float& y, float& z) { x = ax; y = ay; z = az; }
void getGyro(float& x, float& y, float& z) { x = gx; y = gy; z = gz; }
float tiltDeg() { return tilt; }
float roll() { return fRoll; }
float pitch() { return fPitch; }
bool flipped() { return flip; }
void calibrate() { recalReq = true; }
bool calibrated() { return gyroCal; }
float gyroBias(int a) { return a >= 0 && a < 3 ? gbias[a] : 0; }

static bool readG() {
  float x, y, z;
  bus::Guard g;
  if (!qmi.getDataReady()) return false;
  if (!qmi.getAccelerometer(x, y, z)) return false;
  ax = x * scaleToG; ay = y * scaleToG; az = z * scaleToG;
  float a, b, c;
  if (qmi.getGyroscope(a, b, c)) { gx = a - gbias[0]; gy = b - gbias[1]; gz = c - gbias[2]; rawGx = a; rawGy = b; rawGz = c; }
  return true;
}

bool init() {
  bus::Guard g;
  ok = qmi.begin(Wire, I2C_ADDR_QMI8658, PIN_I2C0_SDA, PIN_I2C0_SCL);
  if (!ok) return false;
  qmi.configAccelerometer(SensorQMI8658::ACC_RANGE_4G, SensorQMI8658::ACC_ODR_250Hz,
                          SensorQMI8658::LPF_MODE_0);
  qmi.enableAccelerometer();
  qmi.configGyroscope(SensorQMI8658::GYR_RANGE_512DPS, SensorQMI8658::GYR_ODR_224_2Hz,
                      SensorQMI8658::LPF_MODE_3);
  qmi.enableGyroscope();
  return true;
}

// Average ~0.4 s at boot to learn what "upright" looks like.
static void calibrateNow() {
  float sx = 0, sy = 0, sz = 0; int n = 0;
  for (int i = 0; i < 40; i++) {
    float x, y, z;
    bool got;
    { bus::Guard g; got = qmi.getDataReady() && qmi.getAccelerometer(x, y, z); }
    if (got) { sx += x; sy += y; sz += z; n++; }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  if (!n) return;
  // gyro zero: average the raw gyro over the same time (the robot is standing still)
  {
    float bx = 0, by = 0, bz = 0; int m = 0;
    for (int i = 0; i < 30; i++) {
      float a, b2, c2; bool got;
      { bus::Guard g; got = qmi.getDataReady() && qmi.getGyroscope(a, b2, c2); }
      if (got) { bx += a; by += b2; bz += c2; m++; }
      vTaskDelay(pdMS_TO_TICKS(8));
    }
    if (m > 5) {
      float nx = bx / m, ny = by / m, nz = bz / m;
      if (fabsf(nx) < 20 && fabsf(ny) < 20 && fabsf(nz) < 20) {     // sane = it was really still
        gbias[0] = nx; gbias[1] = ny; gbias[2] = nz; gyroCal = true;
      }
    }
  }
  sx /= n; sy /= n; sz /= n;
  float mag = sqrtf(sx * sx + sy * sy + sz * sz);
  if (mag > 4.f) scaleToG = 1.f / 9.80665f;       // library gave m/s^2
  sx *= scaleToG; sy *= scaleToG; sz *= scaleToG;
  mag = sqrtf(sx * sx + sy * sy + sz * sz);
  if (mag > 0.5f) { refX = sx / mag; refY = sy / mag; refZ = sz / mag; }
}

void update() {
  static bool calibrated = false;
  if (!ok) return;
  if (recalReq) { recalReq = false; calibrated = false; }
  if (!calibrated) { calibrateNow(); calibrated = true; return; }
  if (!readG()) return;
  uint32_t now = millis();
  float mag = sqrtf(ax * ax + ay * ay + az * az);
  float dev = fabsf(mag - 1.f);

  // angle between current gravity and upright reference
  float dot = (ax * refX + ay * refY + az * refZ) / (mag > 0.05f ? mag : 1.f);
  tilt = acosf(constrain(dot, -1.f, 1.f)) * RAD_TO_DEG;

  // --- roll / pitch: gyro for quick motion, accelerometer to stop drift ---
  {
    uint32_t nowUs = micros();
    float dts = lastUs ? (nowUs - lastUs) / 1e6f : 0.01f;
    lastUs = nowUs;
    if (dts > 0.2f) dts = 0.01f;
    float accRoll = atan2f(ay, az) * RAD_TO_DEG;
    float accPitch = atan2f(-ax, sqrtf(ay * ay + az * az)) * RAD_TO_DEG;
    float alpha = (dev < 0.15f) ? 0.96f : 0.995f;          // trust the gyro more while moving
    fRoll = alpha * (fRoll + gx * dts) + (1.f - alpha) * accRoll;
    fPitch = alpha * (fPitch + gy * dts) + (1.f - alpha) * accPitch;
  }
  // --- upside down: gravity points the other way for a while ---
  {
    bool up = (ax * refX + ay * refY + az * refZ) < -0.6f * mag;
    if (up) { if (!flipSince) flipSince = now; if (!flip && now - flipSince > 700) { flip = true; events::publish(events::EV_FLIP, 1); } }
    else { flipSince = 0; if (flip && (ax * refX + ay * refY + az * refZ) > 0.5f * mag) { flip = false; events::publish(events::EV_FLIP, 0); } }
  }

  // --- shake: dynamic acceleration (gravity removed by a slow filter) swings back and forth ---
  {
    static float gl[3] = {0, 0, 1};
    gl[0] += (ax - gl[0]) * 0.04f; gl[1] += (ay - gl[1]) * 0.04f; gl[2] += (az - gl[2]) * 0.04f;
    float d = sqrtf((ax - gl[0]) * (ax - gl[0]) + (ay - gl[1]) * (ay - gl[1]) + (az - gl[2]) * (az - gl[2]));
    float spin = sqrtf(gx * gx + gy * gy + gz * gz);              // deg/s: a twisting shake counts too
    static bool armed = true;
    bool strong = d > 0.55f || spin > 260.f;
    if (strong && armed && now > shakeCooldown) {
      armed = false;
      if (swingCount < 6) swingTimes[swingCount++] = now;
      while (swingCount && now - swingTimes[0] > 900) {
        memmove(swingTimes, swingTimes + 1, (swingCount - 1) * sizeof(uint32_t));
        swingCount--;
      }
      if (swingCount >= 4) {
        swingCount = 0;
        shakeCooldown = now + 1500;
        events::publish(events::EV_SHAKE);
      }
    } else if (d < 0.25f && spin < 120.f) armed = true;           // calm again: ready for the next swing
    if (swingCount && now - swingTimes[swingCount - 1] > 900) swingCount = 0;
  }

  // --- tap: one short sharp spike while resting ---
  if (dev > 0.55f && dev < 1.8f && now > tapCooldown && now - stillSince > 300 && !pickedUp &&
      swingCount <= 1) {
    tapCooldown = now + 600;
    events::publish(events::EV_TAP_IMU);
  }
  if (dev > 0.12f) stillSince = now;

  // --- free fall (weightless) then fall ---
  if (mag < FALL_FREEFALL_G) {
    if (!freefallSince) freefallSince = now;
    if (now - freefallSince > 90 && now > fallCooldown) {
      fallCooldown = now + 3000;
      events::publish(events::EV_FALL);
    }
  } else freefallSince = 0;

  // --- fallen over: tilted far from upright and stays there ---
  if (tilt > 55.f && dev < 0.25f) {
    if (!tiltSince) tiltSince = now;
    if (now - tiltSince > 400 && now > fallCooldown) {
      fallCooldown = now + 5000;
      events::publish(events::EV_FALL);
    }
  } else if (tilt < 40.f) tiltSince = 0;

  // --- pick up: sustained acceleration change after resting ---
  if (!pickedUp) {
    if (dev > 0.18f && now - stillSince < 600) {
      if (!liftSince) liftSince = now;
      if (now - liftSince > 250 && swingCount == 0 && now > shakeCooldown) {
        pickedUp = true;
        events::publish(events::EV_PICKUP);
      }
    } else liftSince = 0;
  } else if (dev < 0.06f && now - stillSince > 1200) {
    pickedUp = false;
    liftSince = 0;
    events::publish(events::EV_PUTDOWN);
  }

  // --- tilt report (rate limited) ---
  float roll = atan2f(ay, az) * RAD_TO_DEG;
  float pitch = atan2f(-ax, sqrtf(ay * ay + az * az)) * RAD_TO_DEG;
  if (now - lastTiltEvt > 500 && (fabsf(roll - lastRoll) > 10 || fabsf(pitch - lastPitch) > 10)) {
    lastRoll = roll; lastPitch = pitch; lastTiltEvt = now;
    events::publish(events::EV_TILT, (int)roll, (int)pitch);
  }
}

}  // namespace imu
