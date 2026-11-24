// storage.cpp - see storage.h
#include "storage.h"
#include "logbuf.h"
#include <Preferences.h>

namespace storage {
static Settings S;
static Preferences prefs;
static const uint32_t MAGIC = 0x524D5431;            // "RMT1"

static void defaults() {
  memset(&S, 0, sizeof(S));
  S.magic = MAGIC;
  for (auto& c : S.cal) c = {300, 3800};
}

void init() {
  prefs.begin("remote", false);
  defaults();
  if (prefs.getBytesLength("s") == sizeof(S)) {
    Settings tmp;
    prefs.getBytes("s", &tmp, sizeof(tmp));
    if (tmp.magic == MAGIC) S = tmp;
  }
  S.session++;                                       // every boot is a new session: old recorded frames can never be replayed
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
}
}  // namespace storage
