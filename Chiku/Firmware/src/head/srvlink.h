// srvlink.h - client for the hub server (Server/server.py). Keeps a copy of the
// server's device list (lamp, fan, ...) that the dashboard screens show and
// change. Changes are sent on the next sync (about once a second); the server
// replies with the full state, so phone, web page and robot stay equal.
// Own low priority task. Does nothing until a server URL is set.
#pragma once
#include <Arduino.h>

namespace srvlink {
enum Type : uint8_t { T_LIGHT, T_FAN, T_SWITCH, T_CLIMATE, T_OTHER };

struct Device {
  char id[16];
  char name[24];
  Type type;
  bool on;
  int level;        // light/fan: 0..100 %, climate: set point in degC
  int temp;         // light: colour temperature in K (0 = not adjustable)
  float load;       // W, 0 = unknown
};

static const int MAX_DEVICES = 8;

void init();                          // creates the task
bool enabled();                       // a server URL is configured
bool linked();                        // last sync worked (< 5 s ago)
uint32_t ageMs();                     // time since last good sync
int count();
bool get(int i, Device& out);         // copy of device i
int find(Type t, int nth = 0);        // index of the nth device of a type, -1 = none
void setOn(int i, bool on);
void setLevel(int i, int level);      // also switches on when level > 0
void toggle(int i);
float envTemp();                      // degC from the server (NAN = unknown)
int envHumidity();                    // % (-1 = unknown)
bool aiAvailable();                   // server says AI voice is ready
void syncNow();
}  // namespace srvlink
