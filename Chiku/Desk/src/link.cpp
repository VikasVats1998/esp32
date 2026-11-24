// link.cpp - see link.h
#include "link.h"
#include "buttons.h"
#include "config.h"
#include "height.h"
#include "logbuf.h"
#include "storage.h"
#include "desklink.h"
#include <ArduinoJson.h>
#include <WiFi.h>

namespace desklink_server {

using namespace desklink;

static WebServer* srv = nullptr;
static ChallengeStore store;
static uint32_t pairingUntil = 0;
static uint32_t failTimes[AUTH_FAIL_LIMIT] = {};
static int failIdx = 0;
static uint32_t lockUntil = 0;
static uint32_t lastPairAt = 0;
// request rate limit: at most 30 per second
static uint32_t rateSecond = 0;
static int rateCount = 0;

bool pairingOpen() { return (int32_t)(pairingUntil - millis()) > 0; }
uint32_t pairingLeftMs() { return pairingOpen() ? pairingUntil - millis() : 0; }
bool lockedOut() { return (int32_t)(lockUntil - millis()) > 0; }

size_t clientCount() {
  size_t n = 0;
  for (auto& c : storage::get().clients) n += c.used;
  return n;
}

static PairedClient* findClient(const char* id) {
  if (!id) return nullptr;
  for (auto& c : storage::get().clients) if (c.used && !strcmp(c.id, id)) return &c;
  return nullptr;
}

static bool rateOk() {
  uint32_t sec = millis() / 1000;
  if (sec != rateSecond) { rateSecond = sec; rateCount = 0; }
  return ++rateCount <= 30;
}

// Five failed messages within a minute lock all commands for a minute (stops guessing).
static void authFailed(const char* why) {
  uint32_t now = millis();
  failTimes[failIdx] = now ? now : 1;
  failIdx = (failIdx + 1) % AUTH_FAIL_LIMIT;
  bool all = true;
  for (uint32_t t : failTimes) if (!t || now - t > AUTH_FAIL_WINDOW_MS) all = false;
  LOGW("link", "rejected message: %s", why);
  if (all) {
    lockUntil = now + AUTH_LOCKOUT_MS;
    memset(failTimes, 0, sizeof(failTimes));
    LOGE("link", "too many bad messages: commands locked for %u s", (unsigned)(AUTH_LOCKOUT_MS / 1000));
  }
}

// ---------------- operations (inside the sealed message) ----------------
static void fillState(JsonDocument& r) {
  height::Info h = height::get();
  r["h"] = h.valid ? roundf(h.cm * 10) / 10.0f : 0;
  r["hv"] = h.valid;
  r["src"] = h.src;
  r["moving"] = h.moving;
  r["dir"] = buttons::moveDir();
  r["travel"] = buttons::travelling();
  r["disp"] = h.display;
  r["armed"] = storage::get().armed;
  r["phold"] = storage::get().presetHold;
  char keys[9] = "";
  for (int i = 0; i < buttons::K_COUNT; i++) keys[i] = buttons::pressed((buttons::Key)i) ? '#' : '-';
  r["keys"] = keys;
  r["frames"] = h.frames;
  r["bad"] = h.badFrames;
  r["bytes"] = h.bytes;
  r["up"] = millis() / 1000;
  r["rssi"] = WiFi.RSSI();
  r["fw"] = FW_VERSION;
}

static void handleOp(PairedClient& cl, JsonDocument& q, JsonDocument& r) {
  const char* op = q["op"] | "";
  const char* err = nullptr;
  if (!strcmp(op, "state")) { fillState(r); r["ok"] = true; return; }
  if (!strcmp(op, "move")) {
    const char* dir = q["dir"] | "";
    err = buttons::hold(!strcmp(dir, "up") ? buttons::K_UP : buttons::K_DOWN, q["ms"] | 400);
    if (strcmp(dir, "up") && strcmp(dir, "down")) err = "dir must be up or down";
  } else if (!strcmp(op, "stop")) buttons::stop();
  else if (!strcmp(op, "preset")) err = buttons::preset(q["n"] | 0);
  else if (!strcmp(op, "save")) err = buttons::saveMemory(q["n"] | 0);
  else if (!strcmp(op, "tap")) {
    buttons::Key k;
    if (!buttons::keyFromName(q["key"] | "", k)) err = "unknown key";
    else err = buttons::tap(k, q["ms"] | 150);
  } else if (!strcmp(op, "setheight")) { height::calibrate(q["h"] | 0.0f); storage::get().estOffsetCm = height::estimate(); storage::save(); }
  else if (!strcmp(op, "arm")) { storage::get().armed = q["on"] | true; if (!storage::get().armed) buttons::stop(); storage::save(); LOGI("link", "armed = %d", storage::get().armed); }
  else if (!strcmp(op, "cfg")) { if (q["presetHold"].is<bool>()) storage::get().presetHold = q["presetHold"]; storage::save(); }
  else if (!strcmp(op, "log")) {
    static logbuf::Line rec[40];
    uint32_t next = 0;
    size_t n = logbuf::read(q["since"] | 0, rec, 40, next);
    r["next"] = next;
    JsonArray a = r["lines"].to<JsonArray>();
    for (size_t i = 0; i < n; i++) { JsonObject o = a.add<JsonObject>(); o["n"] = rec[i].n; o["t"] = rec[i].ms; o["l"] = rec[i].lvl; o["m"] = rec[i].msg; }
  } else if (!strcmp(op, "raw")) {
    static char text[1200];
    height::rawText(text, sizeof(text));
    r["text"] = text;
  } else if (!strcmp(op, "clients")) {
    JsonArray a = r["clients"].to<JsonArray>();
    for (auto& c : storage::get().clients) if (c.used) { JsonObject o = a.add<JsonObject>(); o["id"] = c.id; o["name"] = c.name; o["you"] = !strcmp(c.id, cl.id); }
  } else if (!strcmp(op, "unpair")) {
    cl.used = false;
    storage::save();
    LOGW("link", "client %s (%s) unpaired itself", cl.id, cl.name);
  } else err = "unknown op";
  r["ok"] = err == nullptr;
  if (err) { r["err"] = err; LOGW("link", "%s refused: %s", op, err); }
}

// ---------------- HTTP handlers ----------------
static void handleChallenge() {
  Challenge c;
  store.issue(c);
  srv->send(200, "application/json", String("{\"c\":\"") + toHex(c.data(), c.size()).c_str() + "\"}");
}

static void handleCmd() {
  if (lockedOut()) { srv->send(429, "text/plain", "locked"); return; }
  if (!rateOk()) { srv->send(429, "text/plain", "slow down"); return; }
  JsonDocument d;
  if (deserializeJson(d, srv->arg("plain"))) { srv->send(400, "text/plain", "bad json"); return; }
  PairedClient* cl = findClient(d["id"] | "");
  Challenge ch;
  std::string blob, plain;
  bool ok = fromHex(std::string(d["c"] | ""), ch) && store.consume(ch);       // the challenge is used up now
  if (!ok) { authFailed("unknown or expired challenge"); srv->send(401, "text/plain", "auth"); return; }
  ok = cl && fromHex(std::string(d["d"] | ""), blob) && open(cl->key, ch, "cmd", blob, plain);
  if (!ok) { authFailed(cl ? "bad tag" : "unknown client"); srv->send(401, "text/plain", "auth"); return; }

  JsonDocument q, r;
  if (deserializeJson(q, plain)) { r["ok"] = false; r["err"] = "bad command json"; }
  else handleOp(*cl, q, r);
  String out;
  serializeJson(r, out);
  std::string sealed = seal(cl->key, ch, "rsp", std::string(out.c_str()));
  srv->send(200, "application/json", String("{\"d\":\"") + toHex(sealed).c_str() + "\"}");
}

static void handlePair() {
  if (!pairingOpen()) { srv->send(403, "text/plain", "pairing is closed: power-cycle the desk to open it"); return; }
  if (millis() - lastPairAt < 1000) { srv->send(429, "text/plain", "slow down"); return; }
  lastPairAt = millis();
  JsonDocument d;
  Pub clientPub;
  if (deserializeJson(d, srv->arg("plain")) || !fromHex(std::string(d["pub"] | ""), clientPub)) { srv->send(400, "text/plain", "bad request"); return; }
  PairedClient* slot = nullptr;
  for (auto& c : storage::get().clients) if (!c.used) { slot = &c; break; }
  if (!slot) { srv->send(507, "text/plain", "all client slots used: unpair one"); return; }
  KeyPair kp;
  Key key;
  if (!generateKeyPair(kp) || !deriveKey(kp, clientPub, clientPub, kp.pub, key)) { srv->send(400, "text/plain", "bad public key"); return; }
  uint8_t idb[4];
  randomBytes(idb, 4);
  slot->used = true;
  strlcpy(slot->id, toHex(idb, 4).c_str(), sizeof(slot->id));
  strlcpy(slot->name, d["name"] | "client", sizeof(slot->name));
  slot->key = key;
  storage::save();
  LOGI("link", "paired %s as %s (%u clients)", slot->name, slot->id, (unsigned)clientCount());
  JsonDocument r;
  r["id"] = slot->id;
  r["pub"] = toHex(kp.pub.data(), PUB_LEN);
  String out;
  serializeJson(r, out);
  srv->send(200, "application/json", out);
}

static void handleInfo() {
  JsonDocument r;
  r["dev"] = "desk";
  r["fw"] = FW_VERSION;
  r["link"] = 1;
  r["pairing"] = pairingOpen();
  r["left"] = pairingLeftMs() / 1000;
  r["clients"] = clientCount();
  r["locked"] = lockedOut();
  String out;
  serializeJson(r, out);
  srv->send(200, "application/json", out);
}

void init(WebServer& server) {
  srv = &server;
  pairingUntil = millis() + PAIRING_WINDOW_MS;
  server.on("/dl/info", HTTP_GET, handleInfo);
  server.on("/dl/challenge", HTTP_GET, handleChallenge);
  server.on("/dl/pair", HTTP_POST, handlePair);
  server.on("/dl/cmd", HTTP_POST, handleCmd);
  LOGI("link", "pairing window open for %u s (power-cycle the desk to open it again)", (unsigned)(PAIRING_WINDOW_MS / 1000));
}

void update() {
  static bool wasOpen = true;
  bool open = pairingOpen();
  if (wasOpen && !open) LOGI("link", "pairing window closed");
  wasOpen = open;
}

}  // namespace desklink_server
