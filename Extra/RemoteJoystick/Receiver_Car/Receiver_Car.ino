// 4-gear-motor car receiver (tank/differential mix).
// Driver: L298N or TB6612 / DRV8833 dual H-bridge. Left motors wired in parallel to channel A, right to channel B.
// Joystick: Y = throttle, X = steering.  Mode = max speed (25/50/75/100 %).  Joystick click = horn/LED.
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include "RemoteProtocol.h"

// ---- Pins (ESP32-C3 Super Mini; see PINS.md) ----
#define PIN_L_FWD  4    // left  IN1
#define PIN_L_REV  5    // left  IN2
#define PIN_R_FWD  6    // right IN3
#define PIN_R_REV  7    // right IN4
#define PIN_AUX    -1   // optional horn / light pin, -1 = none

// Flip if a side spins the wrong way
const bool FLIP_LEFT = false, FLIP_RIGHT = false;
const int  PWM_HZ = 20000, PWM_BITS = 8;
const int  SPEED_STEP[REMOTE_MODES] = {25, 50, 75, 100};

volatile RemotePacket remote;
volatile uint32_t lastRx = 0;

void onRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (len != sizeof(RemotePacket) || data[0] != REMOTE_MAGIC) return;
  memcpy((void *)&remote, data, sizeof(RemotePacket));
  lastRx = millis();
}

// speed -100..100
void motor(int pinF, int pinR, int speed, bool flip) {
  if (flip) speed = -speed;
  int duty = abs(speed) * ((1 << PWM_BITS) - 1) / 100;
  ledcWrite(pinF, speed > 0 ? duty : 0);
  ledcWrite(pinR, speed < 0 ? duty : 0);
}

void drive(int left, int right) {
  motor(PIN_L_FWD, PIN_L_REV, left, FLIP_LEFT);
  motor(PIN_R_FWD, PIN_R_REV, right, FLIP_RIGHT);
}

void setup() {
  Serial.begin(115200);
  int pins[4] = {PIN_L_FWD, PIN_L_REV, PIN_R_FWD, PIN_R_REV};
  for (int p : pins) ledcAttach(p, PWM_HZ, PWM_BITS);
  if (PIN_AUX >= 0) pinMode(PIN_AUX, OUTPUT);
  drive(0, 0);

  WiFi.mode(WIFI_STA);
  esp_wifi_set_channel(REMOTE_CHANNEL, WIFI_SECOND_CHAN_NONE);
  if (esp_now_init() != ESP_OK) { Serial.println("ESP-NOW init failed"); while (1) delay(1000); }
  esp_now_register_recv_cb(onRecv);
}

void loop() {
  if (millis() - lastRx > 300) {            // failsafe: lost signal => stop
    drive(0, 0);
    if (PIN_AUX >= 0) digitalWrite(PIN_AUX, LOW);
    delay(20);
    return;
  }
  RemotePacket r;
  memcpy(&r, (const void *)&remote, sizeof(r));

  int top = SPEED_STEP[r.mode % REMOTE_MODES];
  int throttle = r.y, steer = r.x;
  int left  = constrain(throttle + steer, -100, 100) * top / 100;
  int right = constrain(throttle - steer, -100, 100) * top / 100;
  drive(left, right);
  if (PIN_AUX >= 0) digitalWrite(PIN_AUX, r.joyBtn);
  delay(10);
}
