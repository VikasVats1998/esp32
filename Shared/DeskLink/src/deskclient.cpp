// deskclient.cpp - see deskclient.h
#ifdef ARDUINO
#include "deskclient.h"
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>

using namespace desklink;

static Preferences prefs;

void DeskClient::begin() {
  prefs.begin("desklink", false);
  host_ = prefs.getString("host", "");
  id_ = prefs.getString("id", "");
  String keyHex = prefs.getString("key", "");
  paired_ = host_.length() && id_.length() && fromHex(std::string(keyHex.c_str()), key_);
}

void DeskClient::unpair() {
  paired_ = false;
  key_.fill(0);
  prefs.remove("key");
  prefs.remove("id");
}

// "desk.local" -> IP through mDNS (cached for a minute); a plain IP or other name goes to normal DNS.
bool DeskClient::resolve(IPAddress& ip, String& err) {
  if (ipAt_ && millis() - ipAt_ < 60000) { ip = ip_; return true; }
  String h = host_;
  if (h.endsWith(".local")) {
    ip = MDNS.queryHost(h.substring(0, h.length() - 6), 2000);
    if (ip == IPAddress((uint32_t)0)) { err = "cannot find " + h + " (mDNS)"; return false; }
  } else if (!ip.fromString(h) && !WiFi.hostByName(h.c_str(), ip)) {
    err = "cannot resolve " + h;
    return false;
  }
  ip_ = ip;
  ipAt_ = millis();
  return true;
}

bool DeskClient::pair(const String& host, const char* myName, String& err) {
  host_ = host;
  ipAt_ = 0;
  IPAddress ip;
  if (!resolve(ip, err)) return false;
  KeyPair kp;
  if (!generateKeyPair(kp)) { err = "no random numbers"; return false; }
  JsonDocument req;
  req["name"] = myName;
  req["pub"] = toHex(kp.pub.data(), PUB_LEN);
  String body;
  serializeJson(req, body);
  WiFiClient wc;
  HTTPClient http;
  http.setTimeout(4000);
  if (!http.begin(wc, ip.toString(), 80, "/dl/pair")) { err = "cannot connect"; return false; }
  http.addHeader("Content-Type", "application/json");
  int code = http.POST(body);
  String resp = http.getString();
  http.end();
  if (code != 200) { err = code > 0 ? "desk refused (" + String(code) + "): " + resp : "no answer from the desk"; return false; }
  JsonDocument r;
  Pub serverPub;
  if (deserializeJson(r, resp) || !fromHex(std::string(r["pub"] | ""), serverPub) || !r["id"].is<const char*>()) { err = "bad pairing answer"; return false; }
  Key key;
  if (!deriveKey(kp, serverPub, kp.pub, serverPub, key)) { err = "key agreement failed"; return false; }
  key_ = key;
  id_ = String((const char*)r["id"]);
  paired_ = true;
  prefs.putString("host", host_);
  prefs.putString("id", id_);
  prefs.putString("key", toHex(key_.data(), KEY_LEN).c_str());
  return true;
}

bool DeskClient::call(const JsonDocument& request, JsonDocument& response, String& err, uint32_t timeoutMs) {
  if (!paired_) { err = "not paired"; return false; }
  IPAddress ip;
  if (!resolve(ip, err)) return false;
  WiFiClient wc;
  HTTPClient http;
  http.setTimeout(timeoutMs);
  http.setReuse(false);
  // 1. one-time challenge
  if (!http.begin(wc, ip.toString(), 80, "/dl/challenge")) { err = "cannot connect"; ipAt_ = 0; return false; }
  int code = http.GET();
  String body = http.getString();
  http.end();
  if (code != 200) { err = code > 0 ? "challenge failed (" + String(code) + ")" : "no answer from the desk"; ipAt_ = 0; return false; }
  JsonDocument ch;
  Challenge c;
  if (deserializeJson(ch, body) || !fromHex(std::string(ch["c"] | ""), c)) { err = "bad challenge"; return false; }
  // 2. sealed command
  String plain;
  serializeJson(request, plain);
  std::string sealed = seal(key_, c, "cmd", std::string(plain.c_str()));
  if (sealed.empty()) { err = "encryption failed"; return false; }
  JsonDocument env;
  env["id"] = id_;
  env["c"] = toHex(c.data(), c.size());
  env["d"] = toHex(sealed);
  String out;
  serializeJson(env, out);
  if (!http.begin(wc, ip.toString(), 80, "/dl/cmd")) { err = "cannot connect"; return false; }
  http.addHeader("Content-Type", "application/json");
  code = http.POST(out);
  String resp = http.getString();
  http.end();
  if (code == 401) { err = "desk rejected the key (paired again? unpair and pair)"; return false; }
  if (code == 429) { err = "desk is locked or busy"; return false; }
  if (code != 200) { err = code > 0 ? "desk error " + String(code) : "no answer from the desk"; return false; }
  JsonDocument renv;
  std::string blob, answer;
  if (deserializeJson(renv, resp) || !fromHex(std::string(renv["d"] | ""), blob) || !open(key_, c, "rsp", blob, answer)) {
    err = "answer could not be verified";
    return false;
  }
  if (deserializeJson(response, answer)) { err = "bad answer"; return false; }
  return true;
}
#endif
