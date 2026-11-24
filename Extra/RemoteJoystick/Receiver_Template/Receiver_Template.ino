// Minimal receiver for the universal remote - copy this into any new project.
// Works on any ESP32 with arduino-esp32 3.x. Read `remote` anywhere in loop().
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include "RemoteProtocol.h"

volatile RemotePacket remote;          // latest packet
volatile uint32_t lastRx = 0;          // millis() of last packet

void onRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (len != sizeof(RemotePacket) || data[0] != REMOTE_MAGIC) return;
  memcpy((void *)&remote, data, sizeof(RemotePacket));
  lastRx = millis();
}

bool remoteAlive() { return millis() - lastRx < 300; }   // false => stop everything (failsafe)

void remoteBegin() {
  WiFi.mode(WIFI_STA);
  esp_wifi_set_channel(REMOTE_CHANNEL, WIFI_SECOND_CHAN_NONE);
  if (esp_now_init() != ESP_OK) { Serial.println("ESP-NOW init failed"); return; }
  esp_now_register_recv_cb(onRecv);
}

void setup() {
  Serial.begin(115200);
  remoteBegin();
}

void loop() {
  if (remoteAlive()) {
    Serial.printf("x=%4d y=%4d joy=%d btn=%d mode=%d\n", remote.x, remote.y, remote.joyBtn, remote.btn, remote.mode);
  } else {
    Serial.println("no signal");
  }
  delay(200);
}
