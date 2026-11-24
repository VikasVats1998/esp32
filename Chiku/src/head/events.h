// events.h - tiny event bus. Any task can publish(); every module that called
// subscribe() gets its own small FreeRTOS queue and reads it in its own task.
// Nobody calls another module's internals: they just send events.
#pragma once
#include <Arduino.h>

namespace events {

enum Type : uint8_t {
  EV_NONE = 0,
  // input
  EV_TOUCH_TAP,      // a=x b=y
  EV_TOUCH_LONG,     // a=x b=y
  EV_SWIPE,          // a=+1 left->right swipe, -1 right->left; vertical: a=0, b=-1 up, +1 down
  EV_PAD,            // a=pad id (PAD_*), b=pads::Gesture
  // imu
  EV_SHAKE, EV_PICKUP, EV_PUTDOWN, EV_FALL, EV_TAP_IMU,
  EV_TILT,           // a=roll deg b=pitch deg
  // sonar / cam
  EV_DIST_RAW,       // a=cm (unfiltered, from cam or local sensor)
  EV_DIST,           // a=cm (median filtered)
  EV_OBSTACLE,       // a=cm (crossed below stop distance)
  EV_CAM_STATE,      // a=1 cam link alive, 0 lost
  // time
  EV_ALARM_RING,     // a=alarm id (100+ = timer)
  EV_ALARM_STOP,
  EV_TIMER_DONE,
  EV_TIME_SYNCED,
  // weather / network
  EV_WEATHER,        // new weather data available
  EV_WIFI,           // a=1 connected, 0 not
  EV_SETTINGS,       // settings changed (web / ui)
  // robot control
  EV_MOTION_CMD,     // a=gait id, b=cycles
  EV_LOOK,           // a=x*100 (-100..100) b=y*100
  EV_SCREEN,         // a=screen id to show
  EV_POWER,          // a=power state (0 active,1 dim,2 sleep)
  EV_WAKE,
  EV_DESK,           // a=1 desk rising, 2 lowering, 3 stopped/arrived, 4 desk link lost, 5 paired
  EV_LOUD,           // a=sound level dB (microphones heard a sudden loud sound)
  EV_NOTIFY,         // a=mood id: a notification arrived (text: ui_notify)
  EV_BATTERY,        // a=1 low battery, 2 charging started, 3 back to normal; b=percent
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

// History of the last events (for the phone app / manual testing). Noisy events
// (distance, look, tilt) are not stored: they are in /api/state instead.
struct Record { uint32_t n; uint32_t ms; Type type; int32_t a; int32_t b; };
const char* name(Type type);
// Copies records with n > since (oldest first). Returns how many; 'next' = last n seen.
size_t history(uint32_t since, Record* out, size_t max, uint32_t& next);

}  // namespace events
