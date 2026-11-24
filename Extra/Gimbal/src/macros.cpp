// macros.cpp - see macros.h
#include "macros.h"
#include "logbuf.h"
#include "outputs.h"
#include "remotelink.h"
#include "storage.h"

using namespace remotelink;

namespace macros {

static const char* NAMES[MACRO_COUNT] = {"none", "photo_video", "switch_camera", "active_track", "recenter", "flip", "lock", "sport"};

// Defaults. recenter (trigger x2) and flip (trigger x3) are the usual DJI gimbal gestures; photo/video (M once) and
// switch camera (M twice) are common, but CHECK them against your manual / the Mimo app button settings.
// active_track, lock and sport are NOT guessed: set them with "macro set" once you know the button gesture.
static void defaultsFor(uint8_t id, MacroDef& d) {
  d = {};
  auto add = [&](int key, int press, int gap) { if (d.count < 6) d.steps[d.count++] = {(uint8_t)key, (uint16_t)press, (uint16_t)gap}; };
  switch (id) {
    case MACRO_PHOTO_VIDEO: add(1, 80, 0); break;                                   // M
    case MACRO_SWITCH_CAMERA: add(1, 80, 150); add(1, 80, 0); break;                // M M
    case MACRO_RECENTER: add(2, 80, 150); add(2, 80, 0); break;                     // trigger x2
    case MACRO_FLIP: add(2, 80, 150); add(2, 80, 150); add(2, 80, 0); break;        // trigger x3
    default: break;
  }
}

void resetDefaults() {
  for (uint8_t id = 0; id < MACRO_COUNT; id++) defaultsFor(id, storage::get().macros[id]);
  storage::save();
}

void init() {
  bool any = false;
  for (auto& m : storage::get().macros) any |= m.count > 0;
  if (!any) resetDefaults();
}

const char* name(uint8_t id) { return id < MACRO_COUNT ? NAMES[id] : "?"; }

String describe(uint8_t id) {
  if (id >= MACRO_COUNT) return "?";
  const MacroDef& d = storage::get().macros[id];
  if (!d.count) return "(not configured)";
  String s;
  for (int i = 0; i < d.count; i++) {
    if (i) s += ",";
    s += outputs::keyName(d.steps[i].key);
    s += ":" + String(d.steps[i].pressMs) + ":" + String(d.steps[i].gapMs);
  }
  return s;
}

bool set(uint8_t id, const char* spec, String& err) {
  if (id == 0 || id >= MACRO_COUNT) { err = "macro id 1..7"; return false; }
  MacroDef d = {};
  String s(spec);
  s.trim();
  if (s == "off" || s == "-") { storage::get().macros[id] = d; storage::save(); return true; }
  int pos = 0;
  while (pos <= (int)s.length() && d.count < 6) {
    int comma = s.indexOf(',', pos);
    String part = s.substring(pos, comma < 0 ? s.length() : comma);
    int c1 = part.indexOf(':'), c2 = part.indexOf(':', c1 + 1);
    int bit;
    if (c1 < 0 || !outputs::keyFromName(part.substring(0, c1).c_str(), bit)) { err = "bad key in '" + part + "' (shutter m trigger zoomin zoomout x)"; return false; }
    int press = part.substring(c1 + 1, c2 < 0 ? part.length() : c2).toInt();
    int gap = c2 < 0 ? 0 : part.substring(c2 + 1).toInt();
    if (press < 30 || press > 3000 || gap < 0 || gap > 3000) { err = "press 30..3000 ms, pause 0..3000 ms"; return false; }
    d.steps[d.count++] = {(uint8_t)bit, (uint16_t)press, (uint16_t)gap};
    if (comma < 0) break;
    pos = comma + 1;
  }
  storage::get().macros[id] = d;
  storage::save();
  return true;
}

// ---- sequencer ----
static struct { bool active; uint8_t id, index; bool pressing; uint32_t nextAt; } run = {};
static uint16_t mask = 0;

bool running() { return run.active; }
uint16_t keysMask() { return mask; }

void cancel() {
  if (run.active) LOGI("macro", "cancelled");
  run.active = false;
  mask = 0;
  outputs::setMacroKeys(0);
}

void start(uint8_t id) {
  if (id == 0 || id >= MACRO_COUNT) return;
  const MacroDef& d = storage::get().macros[id];
  if (!d.count) { LOGW("macro", "%s is not configured: set it with 'macro set %u key:press:pause,...'", NAMES[id], id); return; }
  if (run.active) { LOGW("macro", "%s ignored: %s is still running", NAMES[id], NAMES[run.id]); return; }
  run = {true, id, 0, false, millis()};
  LOGI("macro", "run %s: %s", NAMES[id], describe(id).c_str());
}

void update() {
  if (!run.active || (int32_t)(millis() - run.nextAt) < 0) return;
  const MacroDef& d = storage::get().macros[run.id];
  const MacroStep& st = d.steps[run.index];
  uint32_t now = millis();
  if (!run.pressing) {
    mask = 1 << st.key;
    outputs::setMacroKeys(mask);
    run.pressing = true;
    run.nextAt = now + st.pressMs;
  } else {
    mask = 0;
    outputs::setMacroKeys(0);
    run.pressing = false;
    if (++run.index >= d.count) { run.active = false; LOGI("macro", "%s done", NAMES[run.id]); }
    else run.nextAt = now + st.gapMs;
  }
}

}  // namespace macros
