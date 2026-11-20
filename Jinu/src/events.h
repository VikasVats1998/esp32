// events.h - tiny event bus (same idea as in Chiku). Any task can publish(); every module that called
// subscribe() gets its own small FreeRTOS queue and reads it in its own task.
// Nobody calls another module's internals: they just send events.
#pragma once
#include <Arduino.h>

namespace events {

enum Type : uint8_t {
  EV_NONE = 0,
  EV_PAD,            // a=pad id (PAD_*), b=pads::Gesture
  EV_BUTTON,         // BOOT button: a=1 short press, 2 long press
  EV_DIST,           // a=cm (median filtered)
  EV_OBSTACLE,       // a=cm (crossed below stop distance)
  EV_WIFI,           // a=1 connected, 0 not
  EV_SETTINGS,       // settings changed (web)
  EV_MOTION_CMD,     // a=gait id, b=cycles
  EV_POWER,          // a=power state (0 active,1 dim,2 sleep)
  EV_WAKE,
  EV_TYPE_COUNT
};

struct Event {
  Type type;
  int32_t a;
  int32_t b;
};

void init();
// Returns a queue that receives every event. Call once per module.
QueueHandle_t subscribe(uint8_t depth = 16);
// Safe from any task. Never blocks. Returns false if a queue was full.
bool publish(Type type, int32_t a = 0, int32_t b = 0);

// History of the last events (for the phone app / manual testing). Distance
// events are not stored (they are in /api/state).
struct Record { uint32_t n; uint32_t ms; Type type; int32_t a; int32_t b; };
const char* name(Type type);
size_t history(uint32_t since, Record* out, size_t max, uint32_t& next);   // oldest first

}  // namespace events
