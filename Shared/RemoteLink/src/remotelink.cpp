// remotelink.cpp - see remotelink.h
#include "remotelink.h"
#include <string.h>

namespace remotelink {

static void put32(uint8_t* p, uint32_t v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }
static uint32_t get32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }

static bool authenticated(Type t) { return t == T_CONTROL || t == T_STATUS; }

size_t encode(const desklink::Key& key, Type type, uint32_t session, uint32_t seq, const uint8_t* payload, size_t n, uint8_t* out) {
  if (n + HEADER_LEN + MAC_LEN > MAX_FRAME) return 0;
  out[0] = MAGIC;
  out[1] = VERSION;
  out[2] = type;
  put32(out + 3, session);
  put32(out + 7, seq);
  memcpy(out + HEADER_LEN, payload, n);
  uint8_t mac[32];
  if (!desklink::hmacSha256(key.data(), key.size(), out, HEADER_LEN + n, mac)) return 0;
  memcpy(out + HEADER_LEN + n, mac, MAC_LEN);
  return HEADER_LEN + n + MAC_LEN;
}

size_t encodePair(Type type, const uint8_t* payload, size_t n, uint8_t* out) {
  if (n + HEADER_LEN + MAC_LEN > 200) return 0;                      // pairing frames carry a 65 byte key (+ tag)
  out[0] = MAGIC;
  out[1] = VERSION;
  out[2] = type;
  memset(out + 3, 0, 8);
  memcpy(out + HEADER_LEN, payload, n);
  return HEADER_LEN + n;
}

bool decode(const desklink::Key& key, const uint8_t* in, size_t len, Header& h, uint8_t* payload, size_t& n) {
  if (len < HEADER_LEN || in[0] != MAGIC || in[1] != VERSION) return false;
  h.type = static_cast<Type>(in[2]);
  h.session = get32(in + 3);
  h.seq = get32(in + 7);
  if (!authenticated(h.type)) {                                       // pairing frame: no MAC
    n = len - HEADER_LEN;
    if (n > 200) return false;
    memcpy(payload, in + HEADER_LEN, n);
    return true;
  }
  if (len < HEADER_LEN + MAC_LEN) return false;
  n = len - HEADER_LEN - MAC_LEN;
  if (n > MAX_FRAME) return false;
  uint8_t mac[32];
  if (!desklink::hmacSha256(key.data(), key.size(), in, HEADER_LEN + n, mac)) return false;
  if (!desklink::constantTimeEqual(mac, in + HEADER_LEN + n, MAC_LEN)) return false;
  memcpy(payload, in + HEADER_LEN, n);
  return true;
}

bool ReplayGuard::accept(uint32_t session, uint32_t seq) {
  if (session > session_) { session_ = session; seq_ = seq; return true; }
  if (session == session_ && seq > seq_) { seq_ = seq; return true; }
  return false;
}

void confirmTag(const desklink::Key& key, const desklink::Pub& clientPub, const desklink::Pub& serverPub, uint8_t out[8]) {
  uint8_t data[7 + 2 * desklink::PUB_LEN];
  memcpy(data, "confirm", 7);
  memcpy(data + 7, clientPub.data(), desklink::PUB_LEN);
  memcpy(data + 7 + desklink::PUB_LEN, serverPub.data(), desklink::PUB_LEN);
  uint8_t mac[32];
  desklink::hmacSha256(key.data(), key.size(), data, sizeof(data), mac);
  memcpy(out, mac, 8);
}

size_t pack(const Control& c, uint8_t* o) {
  o[0] = static_cast<uint8_t>(c.jx); o[1] = static_cast<uint8_t>(c.jy); o[2] = static_cast<uint8_t>(c.zoom); o[3] = c.flags;
  o[4] = c.keys; o[5] = c.keys >> 8; o[6] = c.macro; o[7] = c.macroCount;
  put32(o + 8, c.gsession);
  return 12;
}
bool unpack(const uint8_t* i, size_t n, Control& c) {
  if (n != 12) return false;
  c.jx = static_cast<int8_t>(i[0]); c.jy = static_cast<int8_t>(i[1]); c.zoom = static_cast<int8_t>(i[2]); c.flags = i[3];
  c.keys = i[4] | (i[5] << 8); c.macro = i[6]; c.macroCount = i[7]; c.gsession = get32(i + 8);
  return true;
}
size_t pack(const Status& s, uint8_t* o) { o[0] = s.flags; o[1] = s.battPct; o[2] = static_cast<uint8_t>(s.rssi); o[3] = s.macroDone; return 4; }
bool unpack(const uint8_t* i, size_t n, Status& s) {
  if (n != 4) return false;
  s.flags = i[0]; s.battPct = i[1]; s.rssi = static_cast<int8_t>(i[2]); s.macroDone = i[3];
  return true;
}

}  // namespace remotelink
