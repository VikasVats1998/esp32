// radio.h - ESP-NOW transport for RemoteLink (ESP32 only). Frames are received in the Wi-Fi task and put into a small
// queue; the firmware reads them from its own loop (never decode or do slow work in the callback).
#pragma once
#ifdef ARDUINO
#include <Arduino.h>

namespace radio {

struct RawFrame {
  uint8_t mac[6];
  int8_t rssi;
  uint8_t len;
  uint8_t data[200];
};

bool begin(uint8_t channel);                                   // Wi-Fi station mode (not connected) + ESP-NOW on a fixed channel
bool receive(RawFrame& out);                                   // next received frame, false if none
bool sendTo(const uint8_t mac[6], const uint8_t* data, size_t n);   // unicast (adds the peer on first use)
bool broadcast(const uint8_t* data, size_t n);
void myMac(uint8_t mac[6]);
uint32_t sendFailures();                                       // frames the receiver did not acknowledge
}  // namespace radio
#endif
