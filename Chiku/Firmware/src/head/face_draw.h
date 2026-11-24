// face_draw.h - the drawing half of the face: turns one "frame" (numbers computed by face.cpp)
// into LVGL draw calls (rounded rects with lids, arcs, hearts, stars, spirals, crosses, mouths,
// tears, z z z, battery icon, rings). Pure drawing: no timing, no state.
#pragma once
#include <lvgl.h>

namespace face {

enum EyeKind : uint8_t { K_RECT, K_ARC_UP, K_ARC_DOWN, K_HEART, K_STAR, K_SPIRAL, K_CROSS };
enum MouthKind : uint8_t { MO_NONE, MO_SMILE, MO_FROWN, MO_O, MO_D, MO_FLAT, MO_WAVE, MO_DOTS, MO_TALK, MO_BARS };
enum Extra : uint16_t {
  EX_TEAR = 1, EX_TEARS = 2, EX_SWEAT = 4, EX_ZZZ = 8, EX_QUESTION = 16, EX_BLUSH = 32,
  EX_RING = 64, EX_BATTERY = 128, EX_CHARGE_RING = 256
};

struct EyeFrame {
  uint8_t kind;
  float cx, cy, w, h;      // centre and size (w = diameter for round kinds)
  float radius;            // corner radius (rect)
  float lidIn, lidOut;     // 0..1 of h covered from the top, at the inner / outer side
  float spin;              // spiral rotation in degrees
};

struct Frame {
  EyeFrame eye[2];         // 0 = left, 1 = right
  uint32_t color;          // eye colour
  uint8_t glow;            // glow size in px (0 = off)
  uint8_t mouth;
  float mouthX, mouthY;
  float mouthOpen;         // talking mouth 0..1, bars level 0..1
  uint16_t extras;
  float t;                 // seconds, for small animations (dots, z z z, tears)
  int percent;             // battery / charge %
  const char* text;        // status text under the face ("" = none)
  uint32_t textColor;
  const char* caption;     // small mood name at the bottom ("" = none)
  float bars[9];           // listening bars 0..1
};

// Draw the frame on the layer (called from the face object's draw event).
void drawFrame(lv_layer_t* layer, const Frame& f);
// Screen areas the frame touches (for partial redraw). Returns how many were written (max 'max').
int frameAreas(const Frame& f, lv_area_t* out, int max);

}  // namespace face
