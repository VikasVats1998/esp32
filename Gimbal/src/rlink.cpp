// rlink.cpp - see rlink.h
#include "rlink.h"
#include "config.h"
#include "logbuf.h"
#include "macros.h"
#include "outputs.h"
#include "radio.h"
#include "remotelink.h"
#include "storage.h"

using namespace remotelink;

namespace rlink {

static ReplayGuard guard;
static uint32_t lastRx = 0, lastActive = 0, failsafeAt = 0, lastStatus = 0, pairingUntil = 0, txSeq = 0;
static bool isLinked = false, inFailsafe = true, haveCount = false;
static uint8_t lastCount = 0;
static Stats st = {};

bool linked() { return isLinked; }
bool failsafe() { return inFailsafe; }
bool pairingOpen() { return (int32_t)(pairingUntil - millis()) > 0; }
Stats stats() { st.sendFails = radio::sendFailures(); return st; }

void openPairing() {
  pairingUntil = millis() + PAIRING_WINDOW_MS;
  LOGI("link", "pairing window open for %u s: hold the pairing buttons on the remote", (unsigned)(PAIRING_WINDOW_MS / 1000));
}

void unpair() {
  Settings& s = storage::get();
  s.paired = false;
  s.key.fill(0);
  memset(s.remoteMac, 0, 6);
  storage::save();
  isLinked = false;
  LOGW("link", "unpaired");
}

void init() {
  if (!radio::begin(RADIO_CHANNEL)) LOGE("link", "ESP-NOW failed to start");
  Settings& s = storage::get();
  guard.load(s.remoteSession);
  uint8_t mac[6];
  radio::myMac(mac);
  LOGI("link", "radio channel %d, my address %02X:%02X:%02X:%02X:%02X:%02X", RADIO_CHANNEL, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  openPairing();
}

static void enterFailsafe(const char* why) {
  if (inFailsafe) return;
  inFailsafe = true;
  isLinked = false;
  failsafeAt = millis();
  outputs::releaseAllKeys();
  macros::cancel();
  outputs::centerSticks();                         // hold the centre for a moment, then let go (see update)
  LOGW("link", "FAILSAFE (%s): keys released, sticks centred", why);
}

static void handlePairRequest(const radio::RawFrame& f, const uint8_t* payload, size_t n) {
  if (!pairingOpen()) { LOGW("link", "pairing request ignored: window closed"); return; }
  desklink::Pub clientPub;
  if (n != desklink::PUB_LEN) return;
  memcpy(clientPub.data(), payload, n);
  desklink::KeyPair kp;
  desklink::Key key;
  if (!desklink::generateKeyPair(kp) || !desklink::deriveKey(kp, clientPub, clientPub, kp.pub, key)) { LOGW("link", "pairing: bad public key"); return; }
  Settings& s = storage::get();
  s.paired = true;
  s.key = key;
  memcpy(s.remoteMac, f.mac, 6);
  s.remoteSession = 0;
  guard.load(0);
  storage::save();
  uint8_t body[desklink::PUB_LEN + 8], frame[200];
  memcpy(body, kp.pub.data(), desklink::PUB_LEN);
  confirmTag(key, clientPub, kp.pub, body + desklink::PUB_LEN);
  size_t len = encodePair(T_PAIR_RSP, body, sizeof(body), frame);
  radio::sendTo(f.mac, frame, len);
  pairingUntil = 0;
  isLinked = false;
  haveCount = false;
  LOGI("link", "PAIRED with remote %02X:%02X:%02X:%02X:%02X:%02X", f.mac[0], f.mac[1], f.mac[2], f.mac[3], f.mac[4], f.mac[5]);
}

static void applyControl(const Control& c, int8_t rssi) {
  uint32_t now = millis();
  if (!isLinked) LOGI("link", "remote connected (rssi %d)", rssi);
  isLinked = true;
  inFailsafe = false;
  lastRx = now;
  st.rssi = rssi;
  // macros: a new macroCount starts the macro (the first frame after connecting never does)
  if (!haveCount) { haveCount = true; lastCount = c.macroCount; }
  else if (c.macroCount != lastCount) { lastCount = c.macroCount; macros::start(c.macro); }
  // keys (analog zoom axis turns into the zoom keys when the gimbal's zoom control is two buttons)
  uint16_t keys = c.keys;
  if (!ZOOM_ANALOG) {
    if (c.zoom > 40) keys |= KEY_ZOOM_IN;
    else if (c.zoom < -40) keys |= KEY_ZOOM_OUT;
  }
  outputs::setRemoteKeys(keys);
  // sticks: inject while the remote is active, then centre briefly, then Hi-Z so the physical joystick works
  if (c.flags & F_ACTIVE) { lastActive = now; outputs::setStick(c.jx, c.jy, c.zoom); }
  else if (now - lastActive < IDLE_HIZ_MS) outputs::setStick(0, 0, 0);
  else if (outputs::injecting()) outputs::releaseSticks();
}

void update() {
  uint32_t now = millis();
  radio::RawFrame f;
  while (radio::receive(f)) {
    Header h;
    uint8_t payload[200];
    size_t n = 0;
    Settings& s = storage::get();
    if (f.len >= HEADER_LEN && f.data[0] == MAGIC && f.data[2] == T_PAIR_REQ) {
      if (decode(s.key, f.data, f.len, h, payload, n)) handlePairRequest(f, payload, n);
      continue;
    }
    if (!s.paired) continue;
    if (!decode(s.key, f.data, f.len, h, payload, n)) { st.badMac++; continue; }
    if (h.type != T_CONTROL) continue;
    if (!guard.accept(h.session, h.seq)) { st.replays++; continue; }
    Control c;
    if (!unpack(payload, n, c)) continue;
    if (c.gsession != s.session) { st.wrongSession++; lastStatus = 0; continue; }   // recorded before our last restart (or the remote does not know our session yet): answer with a status frame at once
    if (h.session > s.remoteSession) { s.remoteSession = h.session; storage::save(); }
    st.good++;
    applyControl(c, f.rssi);
  }
  // failsafe
  if (!inFailsafe && now - lastRx > LINK_TIMEOUT_MS) enterFailsafe("no frames");
  if (inFailsafe && outputs::injecting() && now - failsafeAt > FAILSAFE_CENTER_MS) outputs::releaseSticks();
  // status frames back to the remote (5 per second while linked, 1 per second otherwise)
  Settings& s = storage::get();
  if (s.paired && now - lastStatus >= (isLinked ? 200U : 1000U)) {
    lastStatus = now;
    Status sv = {(uint8_t)((inFailsafe ? S_FAILSAFE : 0) | (macros::running() ? S_MACRO_RUNNING : 0) | (outputs::learning() ? S_LEARNING : 0) | (outputs::injecting() ? S_INJECTING : 0)),
                 255, st.rssi, lastCount};
    uint8_t body[8], frame[MAX_FRAME];
    size_t len = encode(s.key, T_STATUS, s.session, ++txSeq, body, pack(sv, body), frame);
    if (len) radio::sendTo(s.remoteMac, frame, len);
  }
}

}  // namespace rlink
