// rlink.cpp - see rlink.h
#include "rlink.h"
#include "config.h"
#include "logbuf.h"
#include "radio.h"
#include "remotelink.h"
#include "storage.h"

using namespace remotelink;

namespace rlink {

static ReplayGuard guard;
static uint32_t txSeq = 0, lastTx = 0, lastStatus = 0, pairUntil = 0, lastPairTx = 0;
static uint32_t gsession = 0;
static Status last = {};
static desklink::KeyPair pairKp;
static bool pairActive = false, pairDone = false, pairOk = false;

bool paired() { return storage::get().paired; }
bool linkOk() { return lastStatus && millis() - lastStatus < LINK_LOST_MS; }
bool gimbalFailsafe() { return last.flags & S_FAILSAFE; }
bool gimbalInjecting() { return last.flags & S_INJECTING; }
bool gimbalMacroRunning() { return last.flags & S_MACRO_RUNNING; }
int8_t rssi() { return last.rssi; }
bool pairing() { return pairActive; }
bool pairingJustFinished(bool& success) { if (!pairDone) return false; pairDone = false; success = pairOk; return true; }

void init() {
  if (!radio::begin(RADIO_CHANNEL)) LOGE("link", "ESP-NOW failed to start");
  Settings& s = storage::get();
  guard.load(s.gimbalSession);
  gsession = s.gimbalSession;
}

void startPairing() {
  if (!desklink::generateKeyPair(pairKp)) { LOGE("link", "no random numbers"); return; }
  pairActive = true;
  pairUntil = millis() + 30000;
  lastPairTx = 0;
  LOGI("link", "pairing: asking for a gimbal module in pairing mode (power-cycle the gimbal first) ...");
}

void unpair() {
  Settings& s = storage::get();
  s.paired = false;
  s.key.fill(0);
  storage::save();
  lastStatus = 0;
  LOGW("link", "unpaired");
}

static void handlePairResponse(const radio::RawFrame& f, const uint8_t* payload, size_t n) {
  if (!pairActive || n != desklink::PUB_LEN + 8) return;
  desklink::Pub serverPub;
  memcpy(serverPub.data(), payload, desklink::PUB_LEN);
  desklink::Key key;
  if (!desklink::deriveKey(pairKp, serverPub, pairKp.pub, serverPub, key)) return;
  uint8_t tag[8];
  confirmTag(key, pairKp.pub, serverPub, tag);
  if (!desklink::constantTimeEqual(tag, payload + desklink::PUB_LEN, 8)) { LOGW("link", "pairing answer failed the key check"); return; }
  Settings& s = storage::get();
  s.paired = true;
  s.key = key;
  memcpy(s.gimbalMac, f.mac, 6);
  s.gimbalSession = 0;
  guard.load(0);
  gsession = 0;
  storage::save();
  pairActive = false;
  pairDone = true;
  pairOk = true;
  lastStatus = 0;
  LOGI("link", "PAIRED with gimbal %02X:%02X:%02X:%02X:%02X:%02X", f.mac[0], f.mac[1], f.mac[2], f.mac[3], f.mac[4], f.mac[5]);
}

static void sendControl(const Control& c) {
  Settings& s = storage::get();
  uint8_t body[16], frame[MAX_FRAME];
  size_t len = encode(s.key, T_CONTROL, s.session, ++txSeq, body, pack(c, body), frame);
  if (len) radio::sendTo(s.gimbalMac, frame, len);
}

void sendNeutral() {
  if (!paired()) return;
  Control c = {};
  c.gsession = gsession;
  sendControl(c);
}

void update(const inputs::State& in, bool idleOnly) {
  uint32_t now = millis();
  Settings& s = storage::get();

  // ---- receive ----
  radio::RawFrame f;
  while (radio::receive(f)) {
    Header h;
    uint8_t payload[200];
    size_t n = 0;
    if (f.len >= HEADER_LEN && f.data[0] == MAGIC && f.data[2] == T_PAIR_RSP) {
      if (decode(s.key, f.data, f.len, h, payload, n)) handlePairResponse(f, payload, n);
      continue;
    }
    if (!s.paired || !decode(s.key, f.data, f.len, h, payload, n) || h.type != T_STATUS) continue;
    if (!guard.accept(h.session, h.seq)) continue;               // replayed or old status frame
    Status sv;
    if (!unpack(payload, n, sv)) continue;
    last = sv;
    lastStatus = now;
    gsession = h.session;                                        // frames of ours are bound to this gimbal boot
    if (h.session > s.gimbalSession) { s.gimbalSession = h.session; storage::save(); }
  }

  // ---- pairing requests ----
  if (pairActive) {
    if ((int32_t)(now - pairUntil) >= 0) { pairActive = false; pairDone = true; pairOk = false; LOGW("link", "pairing timed out"); }
    else if (now - lastPairTx >= 300) {
      lastPairTx = now;
      uint8_t frame[200];
      size_t len = encodePair(T_PAIR_REQ, pairKp.pub.data(), desklink::PUB_LEN, frame);
      radio::broadcast(frame, len);
    }
    return;
  }

  // ---- control frames ----
  if (!s.paired) return;
  uint32_t interval = idleOnly ? TX_IDLE_INTERVAL_MS : TX_INTERVAL_MS;
  if (now - lastTx < interval) return;
  lastTx = now;
  Control c = {};
  c.jx = in.jx; c.jy = in.jy;
  c.flags = (in.stickActive ? F_ACTIVE : 0) | (in.fine ? F_FINE : 0);
  c.keys = in.keys;
  c.macro = in.macro;
  c.macroCount = in.macroCount;
  c.gsession = gsession;
  sendControl(c);
}

}  // namespace rlink
