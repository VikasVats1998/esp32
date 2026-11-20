// camlink.h - UART link to the ESP32-CAM. Line based JSON, one object per line.
//   head -> cam: {"cmd":"snap"} {"cmd":"led","on":true} {"cmd":"sleep"}
//                {"cmd":"wake"} {"cmd":"wifi","ssid":"..","pass":".."} {"cmd":"ip"}
//   cam -> head: {"evt":"ready"} {"evt":"ip","ip":"1.2.3.4"} {"evt":"dist","cm":42}
//                {"evt":"snap","ok":true,"len":12345} {"evt":"cam","state":"on|sleep"}
#pragma once
#include <Arduino.h>

namespace camlink {
void init();
void update();                       // sensor task: reads UART, parses lines
bool alive();                        // heard from the cam in the last 5 s
bool streamReady();
const char* ip();                    // "" if unknown
void streamUrl(char* out, size_t n); // http://ip/stream
void sendCommand(const char* json);  // raw line
void wake();
void sleepCam();
void led(bool on);
void snap();
void sendWifi(const char* ssid, const char* pass);
void askIp();
const char* camState();              // "on", "sleep" or "?"
int lastSnapBytes();
}  // namespace camlink
