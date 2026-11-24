// Universal ESP-NOW joystick remote  -  ESP32-C3 Super Mini
// Sends joystick X/Y (-100..100), joystick click, a button and a mode number to ANY receiver.
// Board: "ESP32C3 Dev Module", USB CDC On Boot: Enabled.  Core: esp32 3.x
//
// Wiring (see PINS.md):  VRx->GPIO0  VRy->GPIO1  SW->GPIO3  Button->GPIO10 (other leg GND)  VCC->3V3  GND->GND
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include "RemoteProtocol.h"

#define PIN_X    0
#define PIN_Y    1
#define PIN_SW   3
#define PIN_BTN  10

// Flip if a direction is inverted for your mounting
const bool INVERT_X = false;
const bool INVERT_Y = true;     // most modules read high when pushed down; true => up = forward
const int  DEADZONE = 8;        // percent
const uint32_t SEND_MS = 20;    // 50 Hz

const uint8_t BROADCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
int centerX = 2048, centerY = 2048;
RemotePacket pkt;
uint8_t mode = 0;

int readAvg(int pin) {
  long s = 0;
  for (int i = 0; i < 16; i++) s += analogRead(pin);
  return s / 16;
}

// map raw ADC (0..4095) around the calibrated centre to -100..100
int8_t scaleAxis(int raw, int center, bool invert) {
  int v = raw >= center ? (raw - center) * 100L / max(1, 4095 - center)
                        : (raw - center) * 100L / max(1, center);
  v = constrain(v, -100, 100);
  if (abs(v) < DEADZONE) v = 0;
  else v = (v > 0 ? 1 : -1) * (abs(v) - DEADZONE) * 100 / (100 - DEADZONE);
  return invert ? -v : v;
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_SW, INPUT_PULLUP);
  pinMode(PIN_BTN, INPUT_PULLUP);
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);   // full 0..3.3V range

  delay(300);                        // let the stick settle, hands off at boot!
  centerX = readAvg(PIN_X);
  centerY = readAvg(PIN_Y);
  Serial.printf("centre X=%d Y=%d\n", centerX, centerY);

  WiFi.mode(WIFI_STA);
  esp_wifi_set_channel(REMOTE_CHANNEL, WIFI_SECOND_CHAN_NONE);
  if (esp_now_init() != ESP_OK) { Serial.println("ESP-NOW init failed"); while (1) delay(1000); }
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, BROADCAST, 6);
  peer.channel = REMOTE_CHANNEL;
  peer.encrypt = false;
  esp_now_add_peer(&peer);
  pkt.magic = REMOTE_MAGIC;
}

bool btnPrev = false;
uint32_t btnDown = 0, lastSend = 0;

void loop() {
  // separate button: short press = next mode, held state also sent as `btn`
  bool b = digitalRead(PIN_BTN) == LOW;
  if (b && !btnPrev) btnDown = millis();
  if (!b && btnPrev && millis() - btnDown > 30 && millis() - btnDown < 600) {
    mode = (mode + 1) % REMOTE_MODES;
    Serial.printf("mode %d\n", mode);
  }
  btnPrev = b;

  if (millis() - lastSend >= SEND_MS) {
    lastSend = millis();
    pkt.x = scaleAxis(readAvg(PIN_X), centerX, INVERT_X);
    pkt.y = scaleAxis(readAvg(PIN_Y), centerY, INVERT_Y);
    pkt.joyBtn = digitalRead(PIN_SW) == LOW;
    pkt.btn = b;
    pkt.mode = mode;
    pkt.seq++;
    esp_now_send(BROADCAST, (uint8_t *)&pkt, sizeof(pkt));

    static uint32_t lastPrint = 0;
    if (millis() - lastPrint > 250) {
      lastPrint = millis();
      Serial.printf("x=%4d y=%4d joy=%d btn=%d mode=%d\n", pkt.x, pkt.y, pkt.joyBtn, pkt.btn, pkt.mode);
    }
  }
}
