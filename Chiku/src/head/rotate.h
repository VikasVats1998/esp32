// rotate.h - turning the picture by any angle (0-359 deg) for the round panel. Pure math, no hardware:
// display.cpp uses it to copy the LVGL frame to the panel, touch.cpp to turn touch points back, and the
// PC test (tools/uisim) checks it. "deg" = how far the picture is turned clockwise on the panel.
// Panel pixel p shows logical pixel  l = c + R(-deg) * (p - c)   (c = centre, nearest pixel).
#pragma once
#include <stdint.h>
#include <math.h>

namespace rot {

struct Turn {
  int deg = 0;
  int32_t cosQ = 65536, sinQ = 0;   // 16.16 fixed point
  float c = 0;                      // centre (both axes, square screen)
};

inline Turn make(int deg, int size) {
  Turn t;
  t.deg = ((deg % 360) + 360) % 360;
  float a = t.deg * 3.14159265f / 180.f;
  t.cosQ = (int32_t)lroundf(cosf(a) * 65536.f);
  t.sinQ = (int32_t)lroundf(sinf(a) * 65536.f);
  t.c = (size - 1) / 2.f;
  return t;
}

// panel -> logical (used for every panel pixel and for touch points)
inline void toLogical(const Turn& t, float px, float py, float& lx, float& ly) {
  float dx = px - t.c, dy = py - t.c, cs = t.cosQ / 65536.f, sn = t.sinQ / 65536.f;
  lx = t.c + cs * dx + sn * dy;
  ly = t.c - sn * dx + cs * dy;
}

// logical -> panel (used to find which panel area a changed logical area lands on)
inline void toPanel(const Turn& t, float lx, float ly, float& px, float& py) {
  float dx = lx - t.c, dy = ly - t.c, cs = t.cosQ / 65536.f, sn = t.sinQ / 65536.f;
  px = t.c + cs * dx - sn * dy;
  py = t.c + sn * dx + cs * dy;
}

// Panel rectangle that covers a logical rectangle after turning (clamped, CO5300: even start, odd end).
inline void panelArea(const Turn& t, int size, int x1, int y1, int x2, int y2, int& ox1, int& oy1, int& ox2, int& oy2) {
  if (t.deg == 0) { ox1 = x1; oy1 = y1; ox2 = x2; oy2 = y2; }
  else {
    float xs[4] = {(float)x1, (float)x2 + 1, (float)x1, (float)x2 + 1}, ys[4] = {(float)y1, (float)y1, (float)y2 + 1, (float)y2 + 1};
    float mnx = 1e9f, mny = 1e9f, mxx = -1e9f, mxy = -1e9f;
    for (int i = 0; i < 4; i++) {
      float px, py;
      toPanel(t, xs[i], ys[i], px, py);
      mnx = fminf(mnx, px); mny = fminf(mny, py); mxx = fmaxf(mxx, px); mxy = fmaxf(mxy, py);
    }
    ox1 = (int)floorf(mnx) - 1; oy1 = (int)floorf(mny) - 1; ox2 = (int)ceilf(mxx) + 1; oy2 = (int)ceilf(mxy) + 1;
  }
  ox1 &= ~1; oy1 &= ~1; ox2 |= 1; oy2 |= 1;
  if (ox1 < 0) ox1 = 0;
  if (oy1 < 0) oy1 = 0;
  if (ox2 > size - 1) ox2 = size - 1;
  if (oy2 > size - 1) oy2 = size - 1;
}

// Fill one panel row segment [x1..x2] at panel row py from the logical frame (RGB565, size x size).
// Pixels whose source is outside the frame become black. Fixed-point stepping: fast enough for the S3.
inline void fillRow(const Turn& t, const uint16_t* frame, int size, int py, int x1, int x2, uint16_t* out) {
  if (t.deg == 0) {
    const uint16_t* src = frame + (int32_t)py * size + x1;
    for (int x = x1; x <= x2; x++) *out++ = *src++;
    return;
  }
  // start point in 16.16, then add (cos, -sin) per pixel to the right
  int32_t cQ = (int32_t)(t.c * 65536.f);
  int32_t dx = (x1 << 16) - cQ, dy = (py << 16) - cQ;
  int64_t lx = (int64_t)cQ + (((int64_t)t.cosQ * dx + (int64_t)t.sinQ * dy) >> 16) + 32768;
  int64_t ly = (int64_t)cQ + ((-(int64_t)t.sinQ * dx + (int64_t)t.cosQ * dy) >> 16) + 32768;
  for (int x = x1; x <= x2; x++) {
    int32_t sx = (int32_t)(lx >> 16), sy = (int32_t)(ly >> 16);
    *out++ = ((uint32_t)sx < (uint32_t)size && (uint32_t)sy < (uint32_t)size) ? frame[sy * size + sx] : 0;
    lx += t.cosQ;
    ly -= t.sinQ;
  }
}

}  // namespace rot
