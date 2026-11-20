// desk.h - the robot's side of the standing-desk accessory: talks to the desk module over the secure
// DeskLink (pairing, sealed commands), keeps its last known state, and publishes EV_DESK events so the
// robot can react (look up while the desk rises, happy when it arrives ...).
// update() blocks for short moments (HTTP), so it runs in the network task only. All other functions
// only set flags and are safe from any task.
#pragma once
#include <Arduino.h>

namespace desk {

struct State {
  bool paired;
  bool ok;            // the last poll worked
  bool hv;            // height is valid
  float h;            // cm
  bool moving;
  int dir;            // +1 up, -1 down, 0 still
  bool armed;
  bool travel;        // a preset travel is running
  char src[10];       // "display", "estimate", "none"
  char err[48];       // last error text, "" if none
  uint32_t ageMs;     // time since the last successful poll
};

void init();
void update();                                  // network task
State state();
const char* host();

bool pair(const char* host, String& err);       // blocking; network task (e.g. from an HTTP handler)
void unpair();

// Access token for the robot's desk HTTP endpoints (the robot holds the desk key, so its API must not be open).
// The phone sets it when it pairs the robot with the desk; later desk commands must send the same token.
bool tokenSet();
void setToken(const char* token);
bool checkToken(const char* token);

// Non-blocking requests (sent by update()):
void moveHold(int dir);                         // call every ~250 ms while a button is held: +1 up, -1 down
void command(const char* op, int n = 0, const char* key = nullptr, float v = 0, int ms = 0);
}  // namespace desk
