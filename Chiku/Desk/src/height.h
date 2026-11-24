// height.h - desk height feedback.
//  1. If the handset cable's data line (controller -> handset) is wired to PIN_DESK_RX, the frames are decoded
//     (Loctek/Jiecang style "F2 F2 cmd len data cs 7E": height value or the 7-segment digits of the display).
//     This is READ ONLY. Every frame is also kept for the capture tool (op "raw") so the decoder can be checked.
//  2. Otherwise the height is estimated from how long UP / DOWN were pressed (calibrate with the real display).
#pragma once
#include <Arduino.h>

namespace height {

struct Info {
  float cm;              // best known height (0 if unknown)
  bool valid;
  const char* src;       // "display", "estimate" or "none"
  bool moving;
  char display[8];       // text of the 3 digit display when it can be read, e.g. "82.0"
  uint32_t frames;       // good frames received
  uint32_t badFrames;    // frames with a wrong checksum
  uint32_t bytes;        // all bytes seen on the line (0 = nothing wired / wrong pin)
};

void init();
void update();                              // control task: parses serial bytes
Info get();
void noteMotion(int dir, uint32_t dtMs);    // from buttons: integrates the estimate
void calibrate(float cm);                   // "the display shows 82.0 right now"
void invalidateEstimate();                  // after a preset travel the estimate is unknown
float estimate();
// Copies the newest captured lines ("HH HH HH ..." per line) into out (newline separated).
size_t rawText(char* out, size_t n);
}  // namespace height
