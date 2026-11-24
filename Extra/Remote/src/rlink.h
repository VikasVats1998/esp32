// rlink.h - the remote's end of RemoteLink: sends control frames (sticks, keys, macros) to the gimbal module and
// receives its status. Also does the pairing.
#pragma once
#include <Arduino.h>
#include "inputs.h"

namespace rlink {
void init();
void update(const inputs::State& in, bool idleOnly);    // call every loop
bool paired();
bool linkOk();                          // status frames from the gimbal arrive
bool gimbalFailsafe();
bool gimbalInjecting();
bool gimbalMacroRunning();
int8_t rssi();                          // as seen by the gimbal (from its last status frame)

void startPairing();                    // broadcasts a request for 30 s
bool pairing();
bool pairingJustFinished(bool& success);
void unpair();
void sendNeutral();                     // one frame with centred stick and no keys (before sleeping)
}  // namespace rlink
