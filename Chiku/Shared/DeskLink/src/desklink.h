// desklink.h - the secure link ("DeskLink v1") used between the desk module and its clients
// (Chiku, Jinu, the phone app). Portable C++ on top of mbedTLS: it also builds on a PC
// (see test/) so it can be tested against the app's JavaScript implementation.
//
// How it works (details in Desk/docs/SECURITY.md):
//  * Pairing (only while the desk module's pairing window is open): ECDH P-256 key exchange,
//    key = HKDF-SHA256(shared secret, info = clientPub || serverPub). Every client gets its own key.
//  * Every request: client asks GET /dl/challenge (16 random bytes, valid 10 s, ONE use), then POSTs
//    the command sealed with AES-256-GCM. The challenge is the AAD, so a recorded request can never be
//    replayed, and the sealed answer is bound to the same challenge.
#pragma once
#include <array>
#include <stdint.h>
#include <string>

namespace desklink {

constexpr size_t KEY_LEN = 32;
constexpr size_t PUB_LEN = 65;        // uncompressed P-256 point (0x04 || X || Y)
constexpr size_t CHALLENGE_LEN = 16;
constexpr size_t IV_LEN = 12;
constexpr size_t TAG_LEN = 16;

using Key = std::array<uint8_t, KEY_LEN>;
using Pub = std::array<uint8_t, PUB_LEN>;
using Challenge = std::array<uint8_t, CHALLENGE_LEN>;

struct KeyPair {
  std::array<uint8_t, 32> priv;
  Pub pub;
};

// ---- platform hooks (implemented in desklink.cpp) ----
bool randomBytes(uint8_t* out, size_t n);
uint32_t nowMs();

// ---- pairing ----
bool generateKeyPair(KeyPair& kp);
bool publicFromPrivate(const std::array<uint8_t, 32>& priv, Pub& pub);     // for tests
// Both sides call this with their own private key and the peer's public key.
bool deriveKey(const KeyPair& mine, const Pub& peer, const Pub& clientPub, const Pub& serverPub, Key& out);

// ---- sealed messages: iv(12) || ciphertext || tag(16) ----
std::string seal(const Key& key, const Challenge& chal, const char* purpose, const std::string& plain);
bool open(const Key& key, const Challenge& chal, const char* purpose, const std::string& blob, std::string& plain);

// ---- helpers ----
std::string toHex(const uint8_t* data, size_t n);
inline std::string toHex(const std::string& s) { return toHex(reinterpret_cast<const uint8_t*>(s.data()), s.size()); }
bool fromHex(const std::string& hex, std::string& out);
template <size_t N>
bool fromHex(const std::string& hex, std::array<uint8_t, N>& out) {
  std::string tmp;
  if (!fromHex(hex, tmp) || tmp.size() != N) return false;
  for (size_t i = 0; i < N; i++) out[i] = static_cast<uint8_t>(tmp[i]);
  return true;
}
std::string sha256(const std::string& data);
// HMAC-SHA256 (32 byte tag)
bool hmacSha256(const uint8_t* key, size_t keyLen, const uint8_t* data, size_t dataLen, uint8_t out[32]);
bool constantTimeEqual(const uint8_t* a, const uint8_t* b, size_t n);

// ---- server side: one-time challenges ----
class ChallengeStore {
 public:
  void issue(Challenge& out);               // makes a new challenge (oldest slot is replaced)
  bool consume(const Challenge& c);         // true once per challenge, and only for 10 s
 private:
  static constexpr int SLOTS = 6;
  static constexpr uint32_t LIFETIME_MS = 10000;
  struct Slot { Challenge c; uint32_t at; bool used; bool valid; };
  Slot slots_[SLOTS] = {};
  int next_ = 0;
};

}  // namespace desklink
