// buttons.h - presses the desk handset's buttons electrically (photorelay / optocoupler across each
// button, driven by a GPIO). All safety rules live here:
//  * UP and DOWN are never pressed together; switching direction waits REVERSE_GAP_MS
//  * a held UP/DOWN is released HOLD_KEEPALIVE_MS after the last hold() call (dead-man: the client must keep sending)
//  * one continuous movement is limited to MAX_RUN_MS, then a short cool-down
//  * nothing moves while "disarmed" (child lock)
//  * every output is LOW at boot, on error and after releaseAll()
#pragma once
#include <Arduino.h>

namespace buttons {

enum Key : uint8_t { K_UP, K_DOWN, K_M1, K_M2, K_M3, K_M4, K_S, K_X, K_COUNT };

const char* keyName(Key k);
bool keyFromName(const char* name, Key& out);   // "up" "down" "1".."4" "s" "x"

void init();                                    // call first thing in setup(): every output LOW
void update();                                  // control task, every 10 ms

// All of these return nullptr on success or a short error text.
const char* hold(Key k, uint32_t ms);           // UP/DOWN: hold now; call again before it ends to keep going
const char* tap(Key k, uint32_t ms);            // any key: press for ms, then release
const char* preset(uint8_t n);                  // go to memory position 1..4
const char* saveMemory(uint8_t n);              // tap S, then tap n (store the current height in n)
void stop();                                    // release everything; stop a preset travel if one runs
void travelDone();                              // height feedback says the preset travel ended

bool pressed(Key k);
int moveDir();                                  // +1 UP held, -1 DOWN held, 0 none
bool travelling();                              // a preset travel is in progress
uint32_t lastActivityMs();
}  // namespace buttons
