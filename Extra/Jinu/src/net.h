// net.h - Wi-Fi (captive portal on first boot), mDNS jinu.local, web page
// (LittleFS), JSON API, OTA update. Runs in its own low priority task.
#pragma once
#include <Arduino.h>

namespace net {
void init();                    // creates the network task
bool connected();
bool portalActive();
const char* ip();               // station IP or "" 
void restartWifi();             // apply new credentials
}  // namespace net
