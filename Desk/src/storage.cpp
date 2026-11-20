// storage.cpp - see storage.h
#include "storage.h"
#include "logbuf.h"
#include <Preferences.h>

namespace storage {
static Settings S;
static Preferences prefs;

void init() {
  memset(&S, 0, sizeof(S));
  S.armed = true;
  S.estOffsetCm = 75.0f;
  prefs.begin("desk", false);
  prefs.getString("ssid", S.ssid, sizeof(S.ssid));
  prefs.getString("pass", S.pass, sizeof(S.pass));
  S.armed = prefs.getBool("armed", true);
  S.presetHold = prefs.getBool("phold", false);
  S.estOffsetCm = prefs.getFloat("est", 75.0f);
  if (prefs.getBytesLength("clients") == sizeof(S.clients)) prefs.getBytes("clients", S.clients, sizeof(S.clients));
  if (prefs.getString("pin", S.pin, sizeof(S.pin)) != 6) {            // first boot: make a random PIN
    uint32_t r;
    desklink::randomBytes(reinterpret_cast<uint8_t*>(&r), sizeof(r));
    snprintf(S.pin, sizeof(S.pin), "%06lu", (unsigned long)(r % 1000000UL));
    prefs.putString("pin", S.pin);
  }
}

Settings& get() { return S; }

void save() {
  prefs.putString("ssid", S.ssid);
  prefs.putString("pass", S.pass);
  prefs.putBool("armed", S.armed);
  prefs.putBool("phold", S.presetHold);
  prefs.putFloat("est", S.estOffsetCm);
  prefs.putBytes("clients", S.clients, sizeof(S.clients));
}

void clearWifi() { S.ssid[0] = 0; S.pass[0] = 0; save(); }
}  // namespace storage
