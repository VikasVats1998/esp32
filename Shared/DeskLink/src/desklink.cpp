// desklink.cpp - see desklink.h
#include "desklink.h"

#include <mbedtls/ecdh.h>
#include <mbedtls/ecp.h>
#include <mbedtls/gcm.h>
#include <mbedtls/md.h>
#include <string.h>

#ifdef ARDUINO
#include <Arduino.h>
#include <esp_random.h>
#else
#include <chrono>
#include <stdio.h>
#endif

namespace desklink {

// ---------------- platform ----------------
bool randomBytes(uint8_t* out, size_t n) {
#ifdef ARDUINO
  esp_fill_random(out, n);                 // hardware RNG (Wi-Fi is on, so it is a true random source)
  return true;
#else
  FILE* f = fopen("/dev/urandom", "rb");
  if (!f) return false;
  bool ok = fread(out, 1, n, f) == n;
  fclose(f);
  return ok;
#endif
}

uint32_t nowMs() {
#ifdef ARDUINO
  return millis();
#else
  using namespace std::chrono;
  return static_cast<uint32_t>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
#endif
}

static int rng(void*, unsigned char* out, size_t n) { return randomBytes(out, n) ? 0 : -1; }

// ---------------- hex / hash ----------------
std::string toHex(const uint8_t* d, size_t n) {
  static const char* H = "0123456789abcdef";
  std::string s;
  s.reserve(n * 2);
  for (size_t i = 0; i < n; i++) { s += H[d[i] >> 4]; s += H[d[i] & 15]; }
  return s;
}

static int nib(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

bool fromHex(const std::string& hex, std::string& out) {
  if (hex.size() % 2) return false;
  out.clear();
  out.reserve(hex.size() / 2);
  for (size_t i = 0; i < hex.size(); i += 2) {
    int a = nib(hex[i]), b = nib(hex[i + 1]);
    if (a < 0 || b < 0) return false;
    out += static_cast<char>((a << 4) | b);
  }
  return true;
}

static bool hmac(const uint8_t* key, size_t kl, const uint8_t* data, size_t dl, uint8_t out[32]) {
  return mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), key, kl, data, dl, out) == 0;
}

std::string sha256(const std::string& data) {
  uint8_t out[32];
  mbedtls_md(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), reinterpret_cast<const uint8_t*>(data.data()), data.size(), out);
  return std::string(reinterpret_cast<char*>(out), 32);
}

bool constantTimeEqual(const uint8_t* a, const uint8_t* b, size_t n) {
  uint8_t d = 0;
  for (size_t i = 0; i < n; i++) d |= a[i] ^ b[i];
  return d == 0;
}

// ---------------- pairing (ECDH P-256 + HKDF-SHA256) ----------------
bool generateKeyPair(KeyPair& kp) {
  mbedtls_ecp_group grp;
  mbedtls_mpi d;
  mbedtls_ecp_point q;
  mbedtls_ecp_group_init(&grp);
  mbedtls_mpi_init(&d);
  mbedtls_ecp_point_init(&q);
  size_t olen = 0;
  bool ok = mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256R1) == 0 &&
            mbedtls_ecp_gen_keypair(&grp, &d, &q, rng, nullptr) == 0 &&
            mbedtls_mpi_write_binary(&d, kp.priv.data(), 32) == 0 &&
            mbedtls_ecp_point_write_binary(&grp, &q, MBEDTLS_ECP_PF_UNCOMPRESSED, &olen, kp.pub.data(), PUB_LEN) == 0 &&
            olen == PUB_LEN;
  mbedtls_ecp_point_free(&q);
  mbedtls_mpi_free(&d);
  mbedtls_ecp_group_free(&grp);
  return ok;
}

bool publicFromPrivate(const std::array<uint8_t, 32>& priv, Pub& pub) {
  mbedtls_ecp_group grp;
  mbedtls_mpi d;
  mbedtls_ecp_point q;
  mbedtls_ecp_group_init(&grp);
  mbedtls_mpi_init(&d);
  mbedtls_ecp_point_init(&q);
  size_t olen = 0;
  bool ok = mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256R1) == 0 &&
            mbedtls_mpi_read_binary(&d, priv.data(), 32) == 0 &&
            mbedtls_ecp_mul(&grp, &q, &d, &grp.G, rng, nullptr) == 0 &&
            mbedtls_ecp_point_write_binary(&grp, &q, MBEDTLS_ECP_PF_UNCOMPRESSED, &olen, pub.data(), PUB_LEN) == 0;
  mbedtls_ecp_point_free(&q);
  mbedtls_mpi_free(&d);
  mbedtls_ecp_group_free(&grp);
  return ok && olen == PUB_LEN;
}

bool deriveKey(const KeyPair& mine, const Pub& peer, const Pub& clientPub, const Pub& serverPub, Key& out) {
  mbedtls_ecp_group grp;
  mbedtls_mpi d, z;
  mbedtls_ecp_point qp;
  mbedtls_ecp_group_init(&grp);
  mbedtls_mpi_init(&d);
  mbedtls_mpi_init(&z);
  mbedtls_ecp_point_init(&qp);
  uint8_t secret[32];
  bool ok = mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256R1) == 0 &&
            mbedtls_mpi_read_binary(&d, mine.priv.data(), 32) == 0 &&
            mbedtls_ecp_point_read_binary(&grp, &qp, peer.data(), PUB_LEN) == 0 &&
            mbedtls_ecp_check_pubkey(&grp, &qp) == 0 &&                       // reject invalid points
            mbedtls_ecdh_compute_shared(&grp, &z, &qp, &d, rng, nullptr) == 0 &&
            mbedtls_mpi_write_binary(&z, secret, 32) == 0;
  mbedtls_ecp_point_free(&qp);
  mbedtls_mpi_free(&z);
  mbedtls_mpi_free(&d);
  mbedtls_ecp_group_free(&grp);
  if (!ok) return false;
  // HKDF-SHA256 (RFC 5869), one output block: PRK = HMAC(salt, secret); K = HMAC(PRK, info || 0x01)
  static const char salt[] = "desklink-v1";
  uint8_t prk[32];
  if (!hmac(reinterpret_cast<const uint8_t*>(salt), sizeof(salt) - 1, secret, 32, prk)) return false;
  uint8_t info[2 * PUB_LEN + 1];
  memcpy(info, clientPub.data(), PUB_LEN);
  memcpy(info + PUB_LEN, serverPub.data(), PUB_LEN);
  info[2 * PUB_LEN] = 0x01;
  bool r = hmac(prk, 32, info, sizeof(info), out.data());
  memset(secret, 0, sizeof(secret));
  memset(prk, 0, sizeof(prk));
  return r;
}

// ---------------- sealed messages (AES-256-GCM) ----------------
static std::string aad(const Challenge& c, const char* purpose) {
  std::string a(reinterpret_cast<const char*>(c.data()), CHALLENGE_LEN);
  a += purpose;
  return a;
}

std::string seal(const Key& key, const Challenge& chal, const char* purpose, const std::string& plain) {
  uint8_t iv[IV_LEN];
  if (!randomBytes(iv, IV_LEN)) return "";
  std::string a = aad(chal, purpose);
  std::string out(reinterpret_cast<char*>(iv), IV_LEN);
  out.resize(IV_LEN + plain.size() + TAG_LEN);
  mbedtls_gcm_context ctx;
  mbedtls_gcm_init(&ctx);
  bool ok = mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, key.data(), 256) == 0 &&
            mbedtls_gcm_crypt_and_tag(&ctx, MBEDTLS_GCM_ENCRYPT, plain.size(), iv, IV_LEN,
                                      reinterpret_cast<const uint8_t*>(a.data()), a.size(),
                                      reinterpret_cast<const uint8_t*>(plain.data()),
                                      reinterpret_cast<uint8_t*>(&out[IV_LEN]), TAG_LEN,
                                      reinterpret_cast<uint8_t*>(&out[IV_LEN + plain.size()])) == 0;
  mbedtls_gcm_free(&ctx);
  return ok ? out : std::string();
}

bool open(const Key& key, const Challenge& chal, const char* purpose, const std::string& blob, std::string& plain) {
  if (blob.size() < IV_LEN + TAG_LEN) return false;
  size_t n = blob.size() - IV_LEN - TAG_LEN;
  std::string a = aad(chal, purpose);
  plain.assign(n, '\0');
  mbedtls_gcm_context ctx;
  mbedtls_gcm_init(&ctx);
  const uint8_t* b = reinterpret_cast<const uint8_t*>(blob.data());
  bool ok = mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, key.data(), 256) == 0 &&
            mbedtls_gcm_auth_decrypt(&ctx, n, b, IV_LEN, reinterpret_cast<const uint8_t*>(a.data()), a.size(),
                                     b + IV_LEN + n, TAG_LEN, b + IV_LEN, reinterpret_cast<uint8_t*>(&plain[0])) == 0;
  mbedtls_gcm_free(&ctx);
  if (!ok) plain.clear();
  return ok;
}

// ---------------- challenges ----------------
void ChallengeStore::issue(Challenge& out) {
  randomBytes(out.data(), CHALLENGE_LEN);
  uint32_t now = nowMs();
  int pick = -1;
  for (int i = 0; i < SLOTS; i++) {                       // reuse a dead slot first
    if (!slots_[i].valid || slots_[i].used || now - slots_[i].at > LIFETIME_MS) { pick = i; break; }
  }
  if (pick < 0) { pick = next_; next_ = (next_ + 1) % SLOTS; }   // otherwise replace the oldest
  slots_[pick] = {out, now, false, true};
}

bool ChallengeStore::consume(const Challenge& c) {
  uint32_t now = nowMs();
  for (auto& s : slots_) {
    if (!s.valid || s.used) continue;
    if (!constantTimeEqual(s.c.data(), c.data(), CHALLENGE_LEN)) continue;
    s.used = true;                                         // one use only, even if the request later fails
    return now - s.at <= LIFETIME_MS;
  }
  return false;
}

}  // namespace desklink
