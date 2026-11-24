// sim.h - state the fake modules return (the simulator sets it per scene).
#pragma once
#include <stdint.h>
struct SimState {
  uint32_t ms = 0;
  int rotation = 0;
  int sonarCm = 32;
  int battery = 74;
  bool charging = false;
  bool wifi = true;
  float micLevel = 0.6f;
  int ringing = -1;            // alarm id, -1 = not ringing
  int timerLeftMs = 16 * 60000, timerTotalMs = 25 * 60000;
  bool timerOn = true;
  float speech = -1;
};
extern SimState sim;
