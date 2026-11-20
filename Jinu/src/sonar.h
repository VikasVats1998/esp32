// sonar.h - HC-SR04 on GPIO18/19. Non-blocking: the echo pulse is timed by an
// interrupt. Median filter over 5 samples. Events: EV_DIST, EV_OBSTACLE.
#pragma once
#include <Arduino.h>

namespace sonar {
void init();
void update();            // sensor task, every ~10 ms (sends a ping every 60 ms)
int distanceCm();         // -1 = no echo recently
}  // namespace sonar
