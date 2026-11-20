// ui.h - status LED, buzzer and battery of the remote.
#pragma once
#include <Arduino.h>

namespace ui {
void init();
void update();                       // call every loop
void beep(uint16_t hz, uint16_t ms); // short non-blocking tone (needs PIN_BUZZER)
void signalOk();                     // two rising beeps
void signalError();                  // low beep
int batteryMv();
int batteryPercent();
bool batteryLow();
}  // namespace ui
