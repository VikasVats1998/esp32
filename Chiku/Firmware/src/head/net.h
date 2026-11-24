// net.h - Wi-Fi (captive portal on first boot), mDNS chiku.local, web settings
// page (LittleFS), JSON API, OTA update. Runs in its own low priority task.
#pragma once
#include <Arduino.h>

namespace net {
void init();                    // creates the network task
bool connected();
bool portalActive();
const char* ip();               // station IP or "" 
int rssi();                     // Wi-Fi signal in dBm (0 if not connected)
void restartWifi();             // apply new credentials
}  // namespace net
