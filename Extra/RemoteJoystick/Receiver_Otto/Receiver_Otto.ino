// Otto biped receiver: 2 hips + 2 feet servos driven by the universal remote.
// Y > 0 walk forward, Y < 0 backward, X turns (when Y ~ 0) or steers while walking.
// Modes: 0 walk | 1 moonwalk-ish shuffle | 2 swing (dance) | 3 up-down.   Joystick click = jitter/wiggle.
// Gait uses the same Otto-style oscillators as EmoBot. Calibrate trim/reverse like in EmoBot.
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include "RemoteProtocol.h"

// ---- Pins (ESP32-C3 Super Mini; see PINS.md) ----
const int servoPin[4] = {4, 5, 6, 7};      // left hip, right hip, left foot, right foot
int  trim[4]    = {0, 0, 0, 0};            // degrees so Otto stands straight at 90
bool reverse[4] = {false, true, false, true};   // mirrored mounting: flip if a leg moves the wrong way

volatile RemotePacket remote;
volatile uint32_t lastRx = 0;

void onRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (len != sizeof(RemotePacket) || data[0] != REMOTE_MAGIC) return;
  memcpy((void *)&remote, data, sizeof(RemotePacket));
  lastRx = millis();
}

void servoWrite(int i, float deg) {
  deg = constrain(deg, 0, 180);
  float d = reverse[i] ? 180 - deg : deg;
  d = constrain(d + trim[i], 0, 180);
  uint32_t us = 500 + d * 2000 / 180;
  ledcWrite(servoPin[i], us * 16384 / 20000);   // 50 Hz, 14-bit
}

void servosRelax() { for (int i = 0; i < 4; i++) ledcWrite(servoPin[i], 0); }

const float DEG = PI / 180;
float phase = 0;                  // oscillator phase, advanced continuously
uint32_t lastT = 0;

// amplitude A, offset O, phase P (radians) per servo
void gait(float *A, float *O, float *P, int periodMs) {
  uint32_t now = millis();
  phase += 2 * PI * (now - lastT) / periodMs;
  if (phase > 2 * PI) phase -= 2 * PI;
  lastT = now;
  for (int i = 0; i < 4; i++) servoWrite(i, 90 + O[i] + A[i] * sin(phase + P[i]));
}
void set4(float *a, float a0, float a1, float a2, float a3) { a[0] = a0; a[1] = a1; a[2] = a2; a[3] = a3; }

void setup() {
  Serial.begin(115200);
  for (int i = 0; i < 4; i++) ledcAttach(servoPin[i], 50, 14);
  for (int i = 0; i < 4; i++) servoWrite(i, 90);
  WiFi.mode(WIFI_STA);
  esp_wifi_set_channel(REMOTE_CHANNEL, WIFI_SECOND_CHAN_NONE);
  if (esp_now_init() != ESP_OK) { Serial.println("ESP-NOW init failed"); while (1) delay(1000); }
  esp_now_register_recv_cb(onRecv);
  lastT = millis();
}

bool relaxed = false;

void loop() {
  RemotePacket r;
  bool alive = millis() - lastRx < 300;
  memcpy(&r, (const void *)&remote, sizeof(r));
  delay(10);

  if (!alive || (abs(r.x) < 15 && abs(r.y) < 15 && !r.joyBtn && r.mode != 2)) {
    // stand still
    for (int i = 0; i < 4; i++) servoWrite(i, 90);
    lastT = millis();
    return;
  }

  float A[4], O[4], P[4];
  set4(A, 0, 0, 0, 0); set4(O, 0, 0, 0, 0); set4(P, 0, 0, 0, 0);
  int dir = r.y >= 0 ? 1 : -1;
  int period = map(constrain(abs(r.y), 15, 100), 15, 100, 1400, 700);   // push further = walk faster

  if (r.joyBtn) {                                   // jitter / wiggle
    set4(A, 15, 15, 0, 0); set4(P, -90 * DEG, 90 * DEG, 0, 0);
    gait(A, O, P, 300);
    return;
  }

  switch (r.mode) {
    case 0:   // walk / turn
      if (abs(r.y) < 15) {                          // spin on the spot
        int d = r.x > 0 ? 1 : -1;
        set4(A, d > 0 ? 30 : 10, d > 0 ? 10 : 30, 20, 20);
        set4(O, 0, 0, 4, -4); set4(P, 0, 0, -90 * DEG, -90 * DEG);
        gait(A, O, P, 1000);
      } else {                                      // walk, X steers by shrinking one hip
        float l = 30 - constrain(r.x, 0, 100) * 0.2f;    // turning right: left hip bigger... right hip smaller
        float rr = 30 - constrain(-r.x, 0, 100) * 0.2f;
        set4(A, rr, l, 20, 20);
        set4(O, 0, 0, 4, -4); set4(P, 0, 0, -90 * dir * DEG, -90 * dir * DEG);
        gait(A, O, P, period);
      }
      break;
    case 1:   // moonwalk shuffle
      set4(A, 25, 25, 25, 25); set4(O, 0, 0, 0, 0);
      set4(P, 0, 120 * DEG * dir, 0, 120 * DEG * dir);
      gait(A, O, P, 1200);
      break;
    case 2:   // swing dance
      set4(A, 0, 0, 20, 20); set4(O, 0, 0, 10, -10);
      gait(A, O, P, 800);
      break;
    case 3:   // up-down
      set4(A, 0, 0, 20, 20); set4(O, 0, 0, 20, -20); set4(P, 0, 0, -90 * DEG, 90 * DEG);
      gait(A, O, P, 900);
      break;
  }
}
