// rlink.h - the gimbal module's end of RemoteLink: receives the remote's frames over ESP-NOW, checks them
// (MAC, replay, gimbal session), applies sticks / keys / macros, and runs the failsafe.
#pragma once
#include <Arduino.h>

namespace rlink {
void init();
void update();                      // call every loop (about 1 kHz)
bool linked();                      // valid frames arrive
bool failsafe();
bool pairingOpen();
void openPairing();                 // pairing window again (console command)
void unpair();

struct Stats { uint32_t good, badMac, replays, wrongSession; int8_t rssi; uint32_t sendFails; };
Stats stats();
}  // namespace rlink
