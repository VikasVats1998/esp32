// Shared packet format for the universal ESP-NOW remote.
// Keep an identical copy of this file next to every sketch (Arduino needs it in the sketch folder).
#pragma once
#include <stdint.h>

#define REMOTE_MAGIC   0xA5
#define REMOTE_CHANNEL 1       // Wi-Fi channel; must match on remote and receiver
#define REMOTE_MODES   4       // mode cycles 0..3 with the separate button

struct __attribute__((packed)) RemotePacket {
  uint8_t magic;   // REMOTE_MAGIC
  int8_t  x;       // joystick X: -100 (left)    .. +100 (right), 0 = centre
  int8_t  y;       // joystick Y: -100 (back)    .. +100 (forward), 0 = centre
  uint8_t joyBtn;  // joystick click: 1 while held
  uint8_t btn;     // separate button: 1 while held
  uint8_t mode;    // 0..REMOTE_MODES-1, advanced by a short press of the separate button
  uint8_t seq;     // increments every packet (detect lost packets)
};
