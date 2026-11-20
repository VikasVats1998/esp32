// link.h - DeskLink server: pairing and sealed commands over HTTP (see Shared/DeskLink and docs/SECURITY.md).
//   GET  /dl/info        public: name, firmware, pairing window open?
//   GET  /dl/challenge   one-time challenge
//   POST /dl/pair        only while the pairing window is open (after power-up)
//   POST /dl/cmd         sealed command from a paired client
#pragma once
#include <WebServer.h>

namespace desklink_server {
void init(WebServer& server);
void update();                  // pairing window timeout, lock-out bookkeeping
bool pairingOpen();
uint32_t pairingLeftMs();
size_t clientCount();
bool lockedOut();
}  // namespace desklink_server
