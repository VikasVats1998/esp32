// radio.cpp - see radio.h
#ifdef ARDUINO
#include "radio.h"
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

namespace radio {

static QueueHandle_t rxQ = nullptr;
static volatile uint32_t failures = 0;
static const uint8_t BROADCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

static void onRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  if (!rxQ || len <= 0 || len > (int)sizeof(RawFrame::data)) return;
  RawFrame f;
  memcpy(f.mac, info->src_addr, 6);
  f.rssi = info->rx_ctrl ? info->rx_ctrl->rssi : 0;
  f.len = len;
  memcpy(f.data, data, len);
  xQueueSend(rxQ, &f, 0);                                  // drop when full: old frames are worthless
}

static void onSent(const uint8_t*, esp_now_send_status_t status) {
  if (status != ESP_NOW_SEND_SUCCESS) failures = failures + 1;
}

bool begin(uint8_t channel) {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_ps(WIFI_PS_NONE);                           // lowest latency
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  if (esp_now_init() != ESP_OK) return false;
  rxQ = xQueueCreate(8, sizeof(RawFrame));
  esp_now_register_recv_cb(onRecv);
  esp_now_register_send_cb(onSent);
  esp_now_peer_info_t p = {};
  memcpy(p.peer_addr, BROADCAST, 6);
  p.channel = channel;
  p.encrypt = false;
  esp_now_add_peer(&p);
  return true;
}

bool receive(RawFrame& out) { return rxQ && xQueueReceive(rxQ, &out, 0) == pdTRUE; }

bool sendTo(const uint8_t mac[6], const uint8_t* data, size_t n) {
  if (!esp_now_is_peer_exist(mac)) {
    esp_now_peer_info_t p = {};
    memcpy(p.peer_addr, mac, 6);
    p.channel = 0;                                         // current channel
    p.encrypt = false;
    if (esp_now_add_peer(&p) != ESP_OK) return false;
  }
  return esp_now_send(mac, data, n) == ESP_OK;
}

bool broadcast(const uint8_t* data, size_t n) { return esp_now_send(BROADCAST, data, n) == ESP_OK; }

void myMac(uint8_t mac[6]) { WiFi.macAddress(mac); }
uint32_t sendFailures() { return failures; }

}  // namespace radio
#endif
