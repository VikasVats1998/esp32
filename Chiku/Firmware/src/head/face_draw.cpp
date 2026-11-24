// face_draw.cpp - see face_draw.h. Coordinates are screen pixels (466 x 466, centre 233).
// Glow: rect eyes use the LVGL shadow (soft); other shapes get two darker "halo" copies behind
// them (on a black background a darker colour looks the same as transparency, without overlap seams).
#include "face_draw.h"
#include "config.h"
#include "ui_common.h"
#include <math.h>

namespace face {

// ---------- small drawing helpers ----------
static lv_layer_t* L = nullptr;

static uint32_t dim(uint32_t c, float k) {
  uint32_t r = (uint32_t)(((c >> 16) & 255) * k), g = (uint32_t)(((c >> 8) & 255) * k), b = (uint32_t)((c & 255) * k);
  return (r << 16) | (g << 8) | b;
}

static void box(float x1, float y1, float x2, float y2, uint32_t c, float radius, int shadow = 0) {
  lv_draw_rect_dsc_t d;
  lv_draw_rect_dsc_init(&d);
  d.bg_color = lv_color_hex(c);
  d.radius = (int32_t)radius;
  if (shadow > 0) {
    d.shadow_width = shadow;
    d.shadow_color = lv_color_hex(c);
    d.shadow_opa = 140;
  }
  lv_area_t a = {(int32_t)lroundf(x1), (int32_t)lroundf(y1), (int32_t)lroundf(x2) - 1, (int32_t)lroundf(y2) - 1};
  if (a.x2 < a.x1 || a.y2 < a.y1) return;
  lv_draw_rect(L, &d, &a);
}

static void ring(float cx, float cy, float w, float h, int border, uint32_t c) {
  lv_draw_rect_dsc_t d;
  lv_draw_rect_dsc_init(&d);
  d.bg_opa = LV_OPA_TRANSP;
  d.border_color = lv_color_hex(c);
  d.border_width = border;
  d.radius = LV_RADIUS_CIRCLE;
  lv_area_t a = {(int32_t)(cx - w / 2), (int32_t)(cy - h / 2), (int32_t)(cx + w / 2), (int32_t)(cy + h / 2)};
  lv_draw_rect(L, &d, &a);
}

static void dot(float cx, float cy, float r, uint32_t c) { box(cx - r, cy - r, cx + r, cy + r, c, LV_RADIUS_CIRCLE); }

static void tri(float ax, float ay, float bx, float by, float cx, float cy, uint32_t c) {
  lv_draw_triangle_dsc_t d;
  lv_draw_triangle_dsc_init(&d);
  d.bg_color = lv_color_hex(c);
  d.p[0] = {(int32_t)lroundf(ax), (int32_t)lroundf(ay)};
  d.p[1] = {(int32_t)lroundf(bx), (int32_t)lroundf(by)};
  d.p[2] = {(int32_t)lroundf(cx), (int32_t)lroundf(cy)};
  lv_draw_triangle(L, &d);
}

static void line(float x1, float y1, float x2, float y2, float w, uint32_t c) {
  lv_draw_line_dsc_t d;
  lv_draw_line_dsc_init(&d);
  d.color = lv_color_hex(c);
  d.width = (int32_t)w;
  d.round_start = d.round_end = 1;
  d.p1 = {(int32_t)lroundf(x1), (int32_t)lroundf(y1)};
  d.p2 = {(int32_t)lroundf(x2), (int32_t)lroundf(y2)};
  lv_draw_line(L, &d);
}

static void arc(float cx, float cy, float r, float w, float a1, float a2, uint32_t c, bool rounded) {
  if (r < 1 || w < 1) return;
  lv_draw_arc_dsc_t d;
  lv_draw_arc_dsc_init(&d);
  d.color = lv_color_hex(c);
  d.center = {(int32_t)lroundf(cx), (int32_t)lroundf(cy)};
  d.radius = (uint16_t)r;
  d.width = (int32_t)fminf(w, r);
  d.start_angle = (int32_t)a1;
  d.end_angle = (int32_t)a2;
  d.rounded = rounded;
  lv_draw_arc(L, &d);
}

static void text(const char* s, const lv_font_t* font, uint32_t c, float cx, float cy, int w = 400) {
  if (!s || !*s) return;
  lv_draw_label_dsc_t d;
  lv_draw_label_dsc_init(&d);
  d.text = s;
  d.font = font;
  d.color = lv_color_hex(c);
  d.align = LV_TEXT_ALIGN_CENTER;
  int h = lv_font_get_line_height(font);
  lv_area_t a = {(int32_t)(cx - w / 2), (int32_t)(cy - h / 2), (int32_t)(cx + w / 2), (int32_t)(cy + h / 2)};
  lv_draw_label(L, &d, &a);
}

// ---------- eye shapes (g = grow in px, used for the glow halo) ----------
static void heart(float cx, float cy, float w, float g, uint32_t c) {
  float r = w / 4 + g * 0.5f;
  float y = cy - w * 0.12f;
  dot(cx - w / 4, y, r, c);
  dot(cx + w / 4, y, r, c);
  box(cx - w / 4, y, cx + w / 4, y + r * 0.55f, c, 0);
  tri(cx - w / 4 - r * 0.866f, y + r * 0.5f, cx + w / 4 + r * 0.866f, y + r * 0.5f, cx, y + w * 0.62f + g, c);
}

static void star(float cx, float cy, float w, float g, uint32_t c) {
  float R = w / 2 + g, ri = w * 0.24f + g * 0.6f;
  dot(cx, cy, ri, c);
  for (int i = 0; i < 5; i++) {
    float a = (-90 + i * 72) * DEG_TO_RAD, al = a - 36 * DEG_TO_RAD, ar = a + 36 * DEG_TO_RAD;
    tri(cx + R * cosf(a), cy + R * sinf(a), cx + ri * cosf(al), cy + ri * sinf(al), cx + ri * cosf(ar), cy + ri * sinf(ar), c);
  }
}

static void spiral(float cx, float cy, float w, float spin, float g, uint32_t c) {
  const int N = 44;
  const float turns = 2.4f, R = w / 2;
  float th = w * 0.08f + 2 * g;
  float px = 0, py = 0;
  for (int i = 0; i <= N; i++) {
    float k = (float)i / N;
    float a = (k * turns * 360.f + spin) * DEG_TO_RAD;
    float r = 3 + (R - th / 2) * k;
    float x = cx + r * cosf(a), y = cy + r * sinf(a);
    if (i) line(px, py, x, y, th, c);
    px = x; py = y;
  }
}

static void cross(float cx, float cy, float w, float g, uint32_t c) {
  float a = w * 0.32f, th = w * 0.16f + 2 * g;
  line(cx - a, cy - a, cx + a, cy + a, th, c);
  line(cx - a, cy + a, cx + a, cy - a, th, c);
}

static void arcEye(const EyeFrame& e, bool up, float g, uint32_t c) {
  float r = e.w / 2;
  float th = e.w * 0.22f;
  if (up) arc(e.cx, e.cy + r * 0.35f, r + g, th + 2 * g, 200, 340, c, false);
  else arc(e.cx, e.cy - r * 0.35f, r + g, th + 2 * g, 20, 160, c, false);
}

static void shape(const EyeFrame& e, float g, uint32_t c) {
  switch (e.kind) {
    case K_ARC_UP: arcEye(e, true, g, c); break;
    case K_ARC_DOWN: arcEye(e, false, g, c); break;
    case K_HEART: heart(e.cx, e.cy, e.w, g, c); break;
    case K_STAR: star(e.cx, e.cy, e.w, g, c); break;
    case K_SPIRAL: spiral(e.cx, e.cy, e.w, e.spin, g, c); break;
    case K_CROSS: cross(e.cx, e.cy, e.w, g, c); break;
    default: break;
  }
}

// Rounded rect eye; the lids are black shapes over the top (angry, sad, sleepy...).
static void rectEye(const EyeFrame& e, int side, uint32_t c, int glow) {
  float x1 = e.cx - e.w / 2, x2 = e.cx + e.w / 2, y1 = e.cy - e.h / 2, y2 = e.cy + e.h / 2;
  float r = fminf(e.radius, fminf(e.w, e.h) / 2);
  box(x1, y1, x2, y2, c, r, glow);
  if (e.lidIn < 0.01f && e.lidOut < 0.01f) return;
  // lid line from the inner corner to the outer corner, extended beyond the eye
  float xin = side < 0 ? x2 : x1, xout = side < 0 ? x1 : x2;
  float yin = y1 + e.lidIn * e.h, yout = y1 + e.lidOut * e.h;
  float m = glow + 6;
  float slope = (yout - yin) / (xout - xin);
  float lx = x1 - m, rx = x2 + m;
  float ly = yin + slope * (lx - xin), ry = yin + slope * (rx - xin);
  float top = y1 - m;
  // one box down to the higher lid end + one triangle for the slope (no shared diagonal = no seam)
  float ymin = fminf(ly, ry);
  box(lx, top, rx, ymin + 1, 0, 0);
  if (fabsf(ly - ry) > 0.5f) {
    if (ly > ry) tri(lx, ymin, rx, ymin, lx, ly, 0);
    else tri(lx, ymin, rx, ymin, rx, ry, 0);
  }
}

static void drop(float x, float y, float r, uint32_t c) {      // tear / sweat drop, tip up
  dot(x, y, r, c);
  tri(x - r * 0.92f, y - r * 0.38f, x + r * 0.92f, y - r * 0.38f, x, y - r * 2.3f, c);
}

// ---------- mouths ----------
static void mouth(const Frame& f, uint32_t c) {
  float mx = f.mouthX, my = f.mouthY;
  switch (f.mouth) {
    case MO_SMILE: arc(mx, my - 26, 36, 9, 35, 145, c, true); break;
    case MO_FROWN: arc(mx, my + 24, 28, 8, 215, 325, c, true); break;
    case MO_O: ring(mx, my, 26, 36, 7, c); break;
    case MO_D: arc(mx, my - 14, 36, 36, 0, 180, c, false); break;
    case MO_FLAT: line(mx - 18, my, mx + 18, my, 8, c); break;
    case MO_WAVE: {
      float px = 0, py = 0;
      for (int i = 0; i <= 24; i++) {
        float x = mx - 58 + i * (116.f / 24), y = my + 10 * sinf(i / 24.f * 2.5f * 2 * PI);
        if (i) line(px, py, x, y, 7, c);
        px = x; py = y;
      }
      break;
    }
    case MO_DOTS:
      for (int i = 0; i < 3; i++) dot(mx + (i - 1) * 22, my - 7 * fmaxf(0, sinf(f.t * 6 - i * 0.9f)), 6, c);
      break;
    case MO_TALK: {
      float h = 12 + 34 * f.mouthOpen;
      box(mx - 33, my - h / 2, mx + 33, my + h / 2, c, LV_RADIUS_CIRCLE);
      break;
    }
    case MO_BARS:
      for (int i = 0; i < 9; i++) {
        float h = 12 + 62 * f.bars[i];
        float x = mx + (i - 4) * 19;
        box(x - 5, my - h / 2, x + 5, my + h / 2, c, 5);
      }
      break;
    default: break;
  }
}

// ---------- extras ----------
static void extras(const Frame& f) {
  const EyeFrame& L0 = f.eye[0];
  const EyeFrame& R0 = f.eye[1];
  if (f.extras & EX_RING) arc(233, 233, 216, 3, 0, 360, dim(f.color, 0.4f), false);
  if (f.extras & EX_BLUSH) {
    for (int i = 0; i < 2; i++) {
      const EyeFrame& e = f.eye[i];
      float x = e.cx + (i ? 1 : -1) * e.w * 0.56f, y = e.cy + e.w * 0.3f;
      box(x - 19, y - 7, x + 19, y + 7, 0xC23A6B, LV_RADIUS_CIRCLE);
    }
  }
  if (f.extras & EX_TEAR) {
    float k = fmodf(f.t * 0.7f, 1.f);
    drop(L0.cx - L0.w * 0.32f, L0.cy + L0.h / 2 + 14 + k * 26, 8, 0x7CC8FF);
  }
  if (f.extras & EX_TEARS) {
    for (int i = 0; i < 2; i++) {
      const EyeFrame& e = f.eye[i];
      for (int j = 0; j < 2; j++) {
        float k = fmodf(f.t * 0.9f + j * 0.5f + i * 0.25f, 1.f);
        drop(e.cx + (i ? 1 : -1) * e.w * 0.3f, e.cy + e.h / 2 + 12 + k * 90, 8, dim(0x7CC8FF, 1.f - 0.6f * k));
      }
    }
  }
  if (f.extras & EX_SWEAT) drop(R0.cx + 75 + 2 * sinf(f.t * 3), R0.cy - 78, 11, 0x7CC8FF);
  if (f.extras & EX_ZZZ) {
    static const lv_font_t* FONTS[3] = {&inter_20, &inter_28, &inter_44};
    static const float X[3] = {308, 333, 362}, Y[3] = {168, 136, 96};
    for (int i = 0; i < 3; i++) text(i == 2 ? "Z" : "z", FONTS[i], 0xB46CFF, X[i], Y[i] - 5 * sinf(f.t * 1.6f + i), 60);
  }
  if (f.extras & EX_QUESTION) text("?", &inter_44, 0xFF9F1C, R0.cx + 72, R0.cy - 92 + 4 * sinf(f.t * 2), 60);
  if (f.extras & EX_BATTERY) {
    float x = 233, y = 338;
    uint32_t o = 0xFFA62B;
    box(x - 56, y - 27, x + 50, y + 27, o, 10);
    box(x - 50, y - 21, x + 44, y + 21, 0, 6);
    box(x + 53, y - 10, x + 61, y + 10, o, 3);
    float fill = fmaxf(10, 88 * constrain(f.percent, 0, 100) / 100.f);
    box(x - 46, y - 17, x - 46 + fill, y + 17, f.percent < 15 ? 0xFF4D4D : 0x4ADE80, 4);
  }
  if (f.extras & EX_CHARGE_RING) {
    arc(233, 233, 214, 14, 0, 360, 0x1F262B, false);
    float end = 270 + 360.f * constrain(f.percent, 0, 100) / 100.f;
    if (f.percent > 0) arc(233, 233, 214, 14, 270, end > 360 ? end - 360 : end, 0x4ADE80, false);
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", f.percent);
    text(buf, &inter_44, 0xF2F4F7, 233, 330);
  }
}

// ---------- frame ----------
void drawFrame(lv_layer_t* layer, const Frame& f) {
  L = layer;
  extras(f);
  for (int i = 0; i < 2; i++) {
    const EyeFrame& e = f.eye[i];
    if (e.kind == K_RECT) rectEye(e, i == 0 ? -1 : 1, f.color, f.glow);
    else {
      if (f.glow) {
        shape(e, f.glow * 0.55f, dim(f.color, 0.16f));
        shape(e, f.glow * 0.25f, dim(f.color, 0.36f));
      }
      shape(e, 0, f.color);
    }
  }
  if (f.mouth != MO_NONE) mouth(f, f.color);
  text(f.text, &inter_16, f.textColor, 233, (f.extras & EX_CHARGE_RING) ? 374 : 396);
  text(f.caption, &inter_14, 0x6B7280, 233, 432);
}

static void addArea(lv_area_t* out, int& n, int max, float x1, float y1, float x2, float y2) {
  if (n >= max) return;
  out[n++] = {(int32_t)fmaxf(0, x1), (int32_t)fmaxf(0, y1), (int32_t)fminf(LCD_W - 1, x2), (int32_t)fminf(LCD_H - 1, y2)};
}

int frameAreas(const Frame& f, lv_area_t* out, int max) {
  int n = 0;
  if (f.extras & (EX_RING | EX_CHARGE_RING)) { addArea(out, n, max, 0, 0, LCD_W, LCD_H); return n; }
  float m = f.glow + 12;
  for (int i = 0; i < 2; i++) {
    const EyeFrame& e = f.eye[i];
    float hw = e.w / 2 + m, hh = (e.kind == K_RECT ? e.h : e.w) / 2 + m;
    addArea(out, n, max, e.cx - hw, e.cy - hh, e.cx + hw, e.cy + hh + (e.kind == K_HEART ? e.w * 0.2f : 0));
  }
  if (f.mouth != MO_NONE) addArea(out, n, max, f.mouthX - 100, f.mouthY - 55, f.mouthX + 100, f.mouthY + 55);
  if (f.extras & (EX_TEAR | EX_TEARS | EX_BLUSH))
    addArea(out, n, max, f.eye[0].cx - 90, f.eye[0].cy, f.eye[1].cx + 90, f.eye[0].cy + f.eye[0].h / 2 + 130);
  if (f.extras & (EX_SWEAT | EX_ZZZ | EX_QUESTION)) addArea(out, n, max, 270, 40, 420, 200);
  if (f.extras & EX_BATTERY) addArea(out, n, max, 160, 300, 310, 375);
  if (f.text && *f.text) addArea(out, n, max, 30, 380, 436, 412);
  if (f.caption && *f.caption) addArea(out, n, max, 100, 420, 366, 446);
  return n;
}

}  // namespace face
