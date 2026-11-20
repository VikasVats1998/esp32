// power.h - power states: ACTIVE -> DIM after 30 s -> SLEEP after 5 min idle.
// Any touch / hand near / BOOT button calls activity() and wakes Jinu.
#pragma once
#include <Arduino.h>

namespace power {
enum State : uint8_t { ACTIVE, DIM, SLEEP };
void init();
void update();            // ~5x per second
void activity();
State state();
}  // namespace power
