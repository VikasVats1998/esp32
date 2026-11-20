// ui_common.h - small LVGL helpers shared by all screens.
#pragma once
#include <Arduino.h>
#include <lvgl.h>
#include "config.h"

namespace uic {

static inline lv_obj_t* panel(lv_obj_t* parent) {       // full-screen black container
  lv_obj_t* o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_set_size(o, LCD_W, LCD_H);
  lv_obj_set_pos(o, 0, 0);
  lv_obj_set_style_bg_color(o, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
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

// Set text only when it changed (avoids useless redraws).
static inline void setText(lv_obj_t* l, const char* text) {
  if (strcmp(lv_label_get_text(l), text) != 0) lv_label_set_text(l, text);
}

static inline lv_obj_t* button(lv_obj_t* parent, const char* text, int x, int y, int w, int h,
                               lv_event_cb_t cb, void* user = nullptr, uint32_t color = 0x2A3A55) {
  lv_obj_t* b = lv_button_create(parent);
  lv_obj_set_size(b, w, h);
  lv_obj_set_pos(b, x, y);
  lv_obj_set_style_bg_color(b, lv_color_hex(color), 0);
  lv_obj_set_style_radius(b, 14, 0);
  lv_obj_set_style_shadow_width(b, 0, 0);
  lv_obj_t* l = lv_label_create(b);
  lv_label_set_text(l, text);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_20, 0);
  lv_obj_center(l);
  if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, user);
  return b;
}

static inline void setButtonText(lv_obj_t* b, const char* text) {
  lv_label_set_text(lv_obj_get_child(b, 0), text);
}

}  // namespace uic
