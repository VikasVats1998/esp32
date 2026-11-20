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
float tiltDeg() { return tilt; }

static bool readG() {
  float x, y, z;
  bus::Guard g;
  if (!qmi.getDataReady()) return false;
  if (!qmi.getAccelerometer(x, y, z)) return false;
  ax = x * scaleToG; ay = y * scaleToG; az = z * scaleToG;
  return true;
}

bool init() {
  bus::Guard g;
  ok = qmi.begin(Wire, I2C_ADDR_QMI8658, PIN_I2C0_SDA, PIN_I2C0_SCL);
  if (!ok) return false;
  qmi.configAccelerometer(SensorQMI8658::ACC_RANGE_4G, SensorQMI8658::ACC_ODR_250Hz,
                          SensorQMI8658::LPF_MODE_0);
  qmi.enableAccelerometer();
  return true;
}

// Average ~0.4 s at boot to learn what "upright" looks like.
static void calibrate() {
  float sx = 0, sy = 0, sz = 0; int n = 0;
  for (int i = 0; i < 40; i++) {
    float x, y, z;
    bool got;
    { bus::Guard g; got = qmi.getDataReady() && qmi.getAccelerometer(x, y, z); }
    if (got) { sx += x; sy += y; sz += z; n++; }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  if (!n) return;
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
  if (!calibrated) { calibrate(); calibrated = true; return; }
  if (!readG()) return;
  uint32_t now = millis();
  float mag = sqrtf(ax * ax + ay * ay + az * az);
  float dev = fabsf(mag - 1.f);

  // angle between current gravity and upright reference
  float dot = (ax * refX + ay * refY + az * refZ) / (mag > 0.05f ? mag : 1.f);
  tilt = acosf(constrain(dot, -1.f, 1.f)) * RAD_TO_DEG;

  // --- shake: several strong swings within 700 ms ---
  float jerk = fabsf(mag - lastMag);
  lastMag = mag;
  if (jerk > SHAKE_G_THRESHOLD * 0.5f && now > shakeCooldown) {
    if (swingCount < 6) swingTimes[swingCount++] = now;
    while (swingCount && now - swingTimes[0] > 700) {
      memmove(swingTimes, swingTimes + 1, (swingCount - 1) * sizeof(uint32_t));
      swingCount--;
    }
    if (swingCount >= 4) {
      swingCount = 0;
      shakeCooldown = now + 1500;
      events::publish(events::EV_SHAKE);
    }
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
