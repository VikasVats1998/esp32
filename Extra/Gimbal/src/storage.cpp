// storage.cpp - see storage.h
#include "storage.h"
#include "config.h"
#include "logbuf.h"
#include <Preferences.h>

namespace storage {
static Settings S;
static Preferences prefs;
static const uint32_t MAGIC = 0x474D4231;            // "GMB1"

static void defaults() {
  memset(&S, 0, sizeof(S));
  S.magic = MAGIC;
  for (auto& a : S.axis) a = {CAL_DEFAULT_MIN, CAL_DEFAULT_CENTER, CAL_DEFAULT_MAX};
}

void init() {
  prefs.begin("gimbal", false);
  defaults();
  if (prefs.getBytesLength("s") == sizeof(S)) {
    Settings tmp;
    prefs.getBytes("s", &tmp, sizeof(tmp));
    if (tmp.magic == MAGIC) S = tmp;
  }
  S.session++;                                       // new session id on every boot (replay protection of our status frames)
  prefs.putBytes("s", &S, sizeof(S));
  LOGI("storage", "boot %lu, %s", (unsigned long)S.session, S.paired ? "paired" : "not paired");
}

Settings& get() { return S; }
void save() { prefs.putBytes("s", &S, sizeof(S)); }

void factoryReset() {
  uint32_t session = S.session;
  defaults();
  S.session = session;
  save();
  LOGW("storage", "factory reset: pairing, calibration and macros erased");
}
}  // namespace storage
