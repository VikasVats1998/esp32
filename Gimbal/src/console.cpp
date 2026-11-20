// console.cpp - see console.h. Type "help" in a serial terminal (115200 baud).
#include "console.h"
#include "config.h"
#include "logbuf.h"
#include "macros.h"
#include "outputs.h"
#include "remotelink.h"
#include "rlink.h"
#include "storage.h"
#include <Arduino.h>

namespace console {

static String line;
static uint32_t testUntil = 0;

static const char* HELP =
  "commands:\n"
  "  status                      link, calibration, keys\n"
  "  joy <x> <y> [ms]            drive the stick -127..127 (default 800 ms), then release\n"
  "  zoom <z> [ms]               analog zoom (ZOOM_ANALOG=1)\n"
  "  key <name> [ms]             press a key: shutter m trigger zoomin zoomout x (default 150 ms)\n"
  "  macro run <1-7> | show | set <id> key:ms:pause,... | reset\n"
  "  learn start | stop          measure centre/range from the PHYSICAL joystick (needs the ADC wires)\n"
  "  cal show | set <x|y|z> <min> <center> <max>      (PWM counts 0..1023 = 0..3.3 V)\n"
  "  wiper                       print the voltages on the joystick wiper nodes\n"
  "  pair                        open the pairing window again       unpair      forget the remote\n"
  "  release                     let go of everything               factory     erase all settings\n";

void init() { Serial.println(HELP); }

static void handle(String cmd) {
  cmd.trim();
  if (!cmd.length()) return;
  int sp = cmd.indexOf(' ');
  String c = sp < 0 ? cmd : cmd.substring(0, sp);
  String rest = sp < 0 ? "" : cmd.substring(sp + 1);
  rest.trim();
  Settings& s = storage::get();
  if (c == "help") Serial.println(HELP);
  else if (c == "status") {
    rlink::Stats t = rlink::stats();
    Serial.printf("fw %s, session %lu, %s, link %s%s, pairing window %s\n", FW_VERSION, (unsigned long)s.session, s.paired ? "paired" : "NOT paired",
                  rlink::linked() ? "up" : "down", rlink::failsafe() ? " (failsafe)" : "", rlink::pairingOpen() ? "open" : "closed");
    Serial.printf("frames good %lu, bad mac %lu, replays %lu, wrong session %lu, rssi %d, send fails %lu\n", (unsigned long)t.good, (unsigned long)t.badMac,
                  (unsigned long)t.replays, (unsigned long)t.wrongSession, t.rssi, (unsigned long)t.sendFails);
    const char* an = "xyz";
    for (int a = 0; a < 3; a++) Serial.printf("axis %c: min %u center %u max %u\n", an[a], s.axis[a].min, s.axis[a].center, s.axis[a].max);
    Serial.printf("keys pressed 0x%02X, injecting %d, macro running %d\n", outputs::keysPressed(), outputs::injecting(), macros::running());
  } else if (c == "joy") {
    int x = 0, y = 0, ms = 800;
    sscanf(rest.c_str(), "%d %d %d", &x, &y, &ms);
    outputs::setStick(constrain(x, -127, 127), constrain(y, -127, 127), 0);
    testUntil = millis() + constrain(ms, 50, 5000);
    LOGI("console", "joy %d %d for %d ms", x, y, ms);
  } else if (c == "zoom") {
    int z = 0, ms = 800;
    sscanf(rest.c_str(), "%d %d", &z, &ms);
    outputs::setStick(0, 0, constrain(z, -127, 127));
    testUntil = millis() + constrain(ms, 50, 5000);
  } else if (c == "key") {
    char name[12] = ""; int ms = 150;
    sscanf(rest.c_str(), "%11s %d", name, &ms);
    int bit;
    if (!outputs::keyFromName(name, bit)) { Serial.println("unknown key"); return; }
    outputs::setRemoteKeys(1 << bit);
    testUntil = millis() + constrain(ms, 30, 3000);
    LOGI("console", "key %s for %d ms", name, ms);
  } else if (c == "macro") {
    if (rest.startsWith("run")) macros::start(rest.substring(3).toInt());
    else if (rest.startsWith("show")) { for (uint8_t i = 1; i < remotelink::MACRO_COUNT; i++) Serial.printf("%u %-14s %s\n", i, macros::name(i), macros::describe(i).c_str()); }
    else if (rest.startsWith("reset")) { macros::resetDefaults(); Serial.println("defaults restored"); }
    else if (rest.startsWith("set")) {
      String r = rest.substring(3); r.trim();
      int sp2 = r.indexOf(' ');
      String err;
      if (sp2 < 0 || !macros::set(r.substring(0, sp2).toInt(), r.substring(sp2 + 1).c_str(), err)) Serial.println("error: " + (err.length() ? err : String("macro set <id> key:ms:pause,...")));
      else Serial.println("saved");
    } else Serial.println("macro run|show|set|reset");
  } else if (c == "learn") {
    if (rest == "start") { if (!outputs::canLearn()) Serial.println("ADC pins not wired (PIN_JOY_X_IN/Y_IN)"); else outputs::learnStart(); }
    else if (rest == "stop") Serial.println(outputs::learnStop() ? "calibration saved" : "nothing learned (did the stick move?)");
    else Serial.println("learn start|stop");
  } else if (c == "cal") {
    if (rest.startsWith("set")) {
      char ax = 0; int mn, ce, mx;
      if (sscanf(rest.c_str(), "set %c %d %d %d", &ax, &mn, &ce, &mx) == 4 && (ax == 'x' || ax == 'y' || ax == 'z') && mn < ce && ce < mx && mx <= 1023 && mn >= 0) {
        s.axis[ax - 'x'] = {(uint16_t)mn, (uint16_t)ce, (uint16_t)mx};
        storage::save();
        Serial.println("saved");
      } else Serial.println("cal set <x|y|z> <min> <center> <max>  (0 <= min < center < max <= 1023)");
    } else handle("status");
  } else if (c == "wiper") {
    for (int a = 0; a < 3; a++) Serial.printf("axis %d: %d mV\n", a, outputs::wiperMv(a));
  } else if (c == "pair") rlink::openPairing();
  else if (c == "unpair") rlink::unpair();
  else if (c == "release") { outputs::releaseAllKeys(); outputs::releaseSticks(); macros::cancel(); testUntil = 0; }
  else if (c == "factory") { storage::factoryReset(); Serial.println("done: re-pair the remote"); }
  else Serial.println("unknown command, type help");
}

void update() {
  while (Serial.available()) {
    char ch = Serial.read();
    if (ch == '\n' || ch == '\r') { handle(line); line = ""; }
    else if (line.length() < 120) line += ch;
  }
  if (testUntil && (int32_t)(millis() - testUntil) >= 0) {        // end of a console test: let go
    testUntil = 0;
    if (!rlink::linked()) { outputs::releaseAllKeys(); outputs::releaseSticks(); }
  }
}

}  // namespace console
