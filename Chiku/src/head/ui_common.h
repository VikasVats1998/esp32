// ui_common.h - the UI kit design system and small LVGL helpers shared by all screens.
// Rules from the kit: true black background (AMOLED pixels off), colour = mood, cards #16191E with
// radius 20, cyan accent #00E5FF, Inter font, touch targets >= 44 px, content inside a 420 px circle
// (labels at y <= 420).
#pragma once
#include <Arduino.h>
#include <lvgl.h>
#include "config.h"

LV_FONT_DECLARE(inter_14)
LV_FONT_DECLARE(inter_16)
LV_FONT_DECLARE(inter_20)
LV_FONT_DECLARE(inter_28)
LV_FONT_DECLARE(inter_44)
LV_FONT_DECLARE(inter_72)

namespace uic {

// ---- colours ----
static const uint32_t C_BG = 0x000000, C_CARD = 0x16191E, C_CARD_HI = 0x242A32, C_LINE = 0x2A3038,
                      C_TEXT = 0xF2F4F7, C_MUTED = 0x8B95A3, C_DIM = 0x4B5563,
                      C_CYAN = 0x00E5FF, C_GREEN = 0x4ADE80, C_YELLOW = 0xFFD43B, C_ORANGE = 0xFF9F1C,
                      C_RED = 0xFF4D4D, C_PURPLE = 0xA970FF, C_PINK = 0xFF5FA2, C_BLUE = 0x6E8BFF,
                      C_GREY = 0x9AA4B2;
static const int RADIUS = 20;
static const int CX = LCD_W / 2;

static inline lv_obj_t* plain(lv_obj_t* parent) {          // style-less object
  lv_obj_t* o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
  return o;
}

static inline lv_obj_t* panel(lv_obj_t* parent) {          // full-screen black screen container
  lv_obj_t* o = plain(parent);
  lv_obj_set_size(o, LCD_W, LCD_H);
  lv_obj_set_pos(o, 0, 0);
  lv_obj_set_style_bg_color(o, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  return o;
}

static inline lv_obj_t* label(lv_obj_t* parent, const lv_font_t* font, uint32_t color,
                              const char* text, int y, lv_align_t align = LV_ALIGN_TOP_MID, int x = 0) {
  lv_obj_t* l = lv_label_create(parent);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(l, text);
  lv_obj_align(l, align, x, y);
  return l;
}

static inline lv_obj_t* title(lv_obj_t* parent, const char* text, int y = 62) {
  return label(parent, &inter_20, C_TEXT, text, y);
}

// Set text only when it changed (avoids useless redraws).
static inline void setText(lv_obj_t* l, const char* text) {
  if (strcmp(lv_label_get_text(l), text) != 0) lv_label_set_text(l, text);
}
static inline void setColor(lv_obj_t* l, uint32_t c) { lv_obj_set_style_text_color(l, lv_color_hex(c), 0); }
static inline void show(lv_obj_t* o, bool on) {
  if (on) lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
}

// Rounded card (#16191E, radius 20). Clickable when cb is set.
static inline lv_obj_t* card(lv_obj_t* parent, int x, int y, int w, int h, lv_event_cb_t cb = nullptr,
                             void* user = nullptr, uint32_t color = C_CARD, int radius = RADIUS) {
  lv_obj_t* c = plain(parent);
  lv_obj_set_pos(c, x, y);
  lv_obj_set_size(c, w, h);
  lv_obj_set_style_bg_color(c, lv_color_hex(color), 0);
  lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(c, radius, 0);
  if (cb) {
    lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(c, lv_color_hex(C_CARD_HI), LV_STATE_PRESSED);
    lv_obj_add_event_cb(c, cb, LV_EVENT_CLICKED, user);
  }
  return c;
}

// Pill button with centred text.
static inline lv_obj_t* button(lv_obj_t* parent, const char* text, int x, int y, int w, int h,
                               lv_event_cb_t cb, void* user = nullptr, uint32_t color = C_CARD_HI,
                               uint32_t textColor = C_TEXT, const lv_font_t* font = &inter_16) {
  lv_obj_t* b = card(parent, x, y, w, h, nullptr, nullptr, color, h / 2);
  lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_opa(b, LV_OPA_70, LV_STATE_PRESSED);
  if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, user);
  lv_obj_t* l = lv_label_create(b);
  lv_label_set_text(l, text);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(textColor), 0);
  lv_obj_center(l);
  return b;
}
static inline void setButtonText(lv_obj_t* b, const char* text) {
  lv_obj_t* l = lv_obj_get_child(b, 0);
  if (strcmp(lv_label_get_text(l), text)) lv_label_set_text(l, text);
}

// Round icon button: dark disc with a coloured ring and a symbol (power menu, menu items).
static inline lv_obj_t* circleButton(lv_obj_t* parent, const char* text, int cx, int cy, int d, uint32_t ring,
                                     lv_event_cb_t cb, void* user = nullptr, const lv_font_t* font = &inter_14) {
  lv_obj_t* b = card(parent, cx - d / 2, cy - d / 2, d, d, nullptr, nullptr, 0x1A1E24, LV_RADIUS_CIRCLE);
  lv_obj_set_style_border_width(b, 4, 0);
  lv_obj_set_style_border_color(b, lv_color_hex(ring), 0);
  lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_color(b, lv_color_hex(0x2A3038), LV_STATE_PRESSED);
  if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, user);
  lv_obj_t* l = lv_label_create(b);
  lv_label_set_text(l, text);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(ring), 0);
  lv_obj_center(l);
  return b;
}

// Full circle progress ring (arc widget). value 0..1000, starts at 12 o'clock, clockwise.
static inline lv_obj_t* ring(lv_obj_t* parent, int d, int width, uint32_t color, uint32_t track = 0x1F262B) {
  lv_obj_t* a = lv_arc_create(parent);
  lv_obj_set_size(a, d, d);
  lv_obj_center(a);
  lv_obj_remove_style(a, nullptr, LV_PART_KNOB);
  lv_obj_remove_flag(a, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_arc_width(a, width, LV_PART_MAIN);
  lv_obj_set_style_arc_color(a, lv_color_hex(track), LV_PART_MAIN);
  lv_obj_set_style_arc_width(a, width, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(a, lv_color_hex(color), LV_PART_INDICATOR);
  lv_obj_set_style_arc_rounded(a, false, LV_PART_INDICATOR);
  lv_obj_set_style_arc_rounded(a, false, LV_PART_MAIN);
  lv_arc_set_rotation(a, 270);
  lv_arc_set_bg_angles(a, 0, 360);
  lv_arc_set_range(a, 0, 1000);
  lv_arc_set_value(a, 0);
  return a;
}
static inline void setRing(lv_obj_t* a, int v) { if (lv_arc_get_value(a) != v) lv_arc_set_value(a, v); }

// Progress dots (setup steps): n dots, 'active' first ones lit.
static inline lv_obj_t* dots(lv_obj_t* parent, int n, int y) {
  lv_obj_t* row = plain(parent);
  lv_obj_set_size(row, n * 22, 12);
  lv_obj_align(row, LV_ALIGN_TOP_MID, 0, y);
  for (int i = 0; i < n; i++) {
    lv_obj_t* d = plain(row);
    lv_obj_set_size(d, 10, 10);
    lv_obj_set_pos(d, i * 22 + 6, 1);
    lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(d, lv_color_hex(C_LINE), 0);
  }
  return row;
}
static inline void setDots(lv_obj_t* row, int active) {
  int n = lv_obj_get_child_count(row);
  for (int i = 0; i < n; i++)
    lv_obj_set_style_bg_color(lv_obj_get_child(row, i), lv_color_hex(i < active ? C_CYAN : C_LINE), 0);
}

// Small chip with text (status pills like "LIVE . ESP32-CAM", "< 15 cm -> scared").
static inline lv_obj_t* chip(lv_obj_t* parent, const char* text, int y, uint32_t textColor = C_TEXT,
                             uint32_t bg = C_CARD, const lv_font_t* font = &inter_14) {
  lv_obj_t* c = plain(parent);
  lv_obj_set_size(c, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_color(c, lv_color_hex(bg), 0);
  lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(c, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_pad_hor(c, 14, 0);
  lv_obj_set_style_pad_ver(c, 6, 0);
  lv_obj_align(c, LV_ALIGN_TOP_MID, 0, y);
  lv_obj_t* l = lv_label_create(c);
  lv_label_set_text(l, text);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(textColor), 0);
  return c;
}
static inline lv_obj_t* chipLabel(lv_obj_t* chip) { return lv_obj_get_child(chip, 0); }

// Vertical scroll list inside the circle (settings, alarms, moves).
static inline lv_obj_t* list(lv_obj_t* parent, int y, int h, int w = 330) {
  lv_obj_t* l = plain(parent);
  lv_obj_set_size(l, w, h);
  lv_obj_align(l, LV_ALIGN_TOP_MID, 0, y);
  lv_obj_add_flag(l, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(l, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(l, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_flex_flow(l, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(l, 8, 0);
  lv_obj_set_style_pad_bottom(l, 40, 0);
  return l;
}

// Mini eyes (clock, timer, alarm): two small rounded rects that blink now and then.
struct MiniEyes {
  lv_obj_t* e[2] = {nullptr, nullptr};
  int w = 26, h = 34, y0 = 0;
  uint32_t next = 2500, t = 0;
  void create(lv_obj_t* parent, int y, int ew, int eh, int gap, uint32_t color) {
    w = ew; h = eh; y0 = y;
    for (int i = 0; i < 2; i++) {
      e[i] = plain(parent);
      lv_obj_set_size(e[i], w, h);
      lv_obj_set_style_radius(e[i], w / 3, 0);
      lv_obj_set_style_bg_opa(e[i], LV_OPA_COVER, 0);
      lv_obj_set_style_shadow_width(e[i], 12, 0);
      lv_obj_set_style_shadow_opa(e[i], 110, 0);
      lv_obj_align(e[i], LV_ALIGN_TOP_MID, (i ? 1 : -1) * gap / 2, y);
    }
    setColor(color);
  }
  void setColor(uint32_t c) {
    for (auto o : e) { lv_obj_set_style_bg_color(o, lv_color_hex(c), 0); lv_obj_set_style_shadow_color(o, lv_color_hex(c), 0); }
  }
  void update(uint32_t dt) {
    if (!e[0]) return;
    int hh = h;
    if (t) {
      t += dt;
      float k = t < 100 ? 1 - t / 100.f : (t - 100) / 100.f;
      hh = max(4, (int)(h * k));
      if (t >= 200) { t = 0; next = random(BLINK_MIN_MS, BLINK_MAX_MS); hh = h; }
    } else if (next > dt) next -= dt;
    else t = 1;
    for (auto o : e) if (lv_obj_get_height(o) != hh) { lv_obj_set_height(o, hh); lv_obj_set_y(o, y0 + (h - hh) / 2); }
  }
};

}  // namespace uic
