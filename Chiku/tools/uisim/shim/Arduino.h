// Arduino.h for the PC simulator: just enough of the Arduino / FreeRTOS API for the UI code.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <algorithm>
#include <string>
using std::min;
using std::max;
#define PI 3.1415926535897932384626433832795
#define DEG_TO_RAD 0.017453292519943295769236907684886
#define RAD_TO_DEG 57.295779513082320876798154814105
#define IRAM_ATTR
#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))
uint32_t millis();
void delay(uint32_t ms);
long random(long lo, long hi);
long random(long hi);
typedef void* QueueHandle_t;
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(m) (void)(m)
#define portEXIT_CRITICAL(m) (void)(m)
struct EspClass { void restart() { exit(0); } };
extern EspClass ESP;
class String {
  std::string s;
 public:
  String(int i) : s(std::to_string(i)) {}
  String(const char* c = "") : s(c) {}
  const char* c_str() const { return s.c_str(); }
};
inline void esp_deep_sleep_start() {}
