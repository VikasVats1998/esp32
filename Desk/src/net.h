// net.h - Wi-Fi (setup portal only while no network is saved), mDNS desk.local, web server, OTA (pairing window only).
#pragma once
#include <Arduino.h>

namespace net {
void init();             // starts the network task
bool connected();
bool portal();
const char* ip();
}  // namespace net
