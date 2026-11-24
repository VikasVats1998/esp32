// ui_notify.cpp - notification / alert (UI kit "Doorbell"): big eyes in the mood colour, a card with
// title and text, "Tap to close". Sent by the API (POST /api/notify) from Home Assistant, Node-RED ...
// Closes by itself after 20 s.
#include "ui_screens.h"
#include "ui_common.h"
#include "ui.h"
#include "face.h"
#include "events.h"

namespace ui_notify {
static lv_obj_t *eye[2], *lTitle, *lText, *icon;
static char title[40], text[96];
static volatile uint8_t mood = face::M_SURPRISED;
static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
static uint32_t shownMs = 0;

void post(const char* t, const char* x, uint8_t m) {
  portENTER_CRITICAL(&mux);
  strlcpy(title, t && *t ? t : "Notification", sizeof(title));
  strlcpy(text, x ? x : "", sizeof(text));
  mood = m < face::MOOD_COUNT ? m : face::M_SURPRISED;
  portEXIT_CRITICAL(&mux);
  events::publish(events::EV_NOTIFY, mood);
}

static void onTap(lv_event_t*) { ui::show(ui::S_FACE); }

lv_obj_t* create(lv_obj_t* parent) {
  lv_obj_t* p = uic::panel(parent);
  lv_obj_add_flag(p, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(p, onTap, LV_EVENT_CLICKED, nullptr);
  for (int i = 0; i < 2; i++) {
    eye[i] = uic::plain(p);
    lv_obj_set_size(eye[i], 104, 104);
    lv_obj_set_pos(eye[i], i ? 263 : 99, 82);
    lv_obj_set_style_radius(eye[i], LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(eye[i], LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(eye[i], 20, 0);
    lv_obj_set_style_shadow_opa(eye[i], 120, 0);
  }
  lv_obj_t* c = uic::card(p, 78, 214, 310, 100);
  lv_obj_remove_flag(c, LV_OBJ_FLAG_CLICKABLE);
  icon = uic::plain(c);
  lv_obj_set_size(icon, 34, 34);
  lv_obj_set_pos(icon, 16, 18);
  lv_obj_set_style_radius(icon, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(icon, LV_OPA_COVER, 0);
  lv_obj_t* ex = uic::label(icon, &inter_20, 0x000000, "!", 0, LV_ALIGN_CENTER);
  (void)ex;
  lTitle = uic::label(c, &inter_16, uic::C_TEXT, "", 18, LV_ALIGN_TOP_LEFT, 64);
  lText = uic::label(c, &inter_14, uic::C_MUTED, "", 44, LV_ALIGN_TOP_LEFT, 64);
  lv_obj_set_width(lText, 230);
  lv_obj_set_style_text_align(lText, LV_TEXT_ALIGN_LEFT, 0);
  lv_label_set_long_mode(lText, LV_LABEL_LONG_DOT);
  lv_obj_set_height(lText, 40);
  uic::label(p, &inter_14, uic::C_YELLOW, "Tap to close", 346);
  return p;
}

void update(uint32_t) {
  if (millis() - shownMs > 20000) ui::show(ui::S_FACE);
}

void onShow() {
  char t[40], x[96];
  portENTER_CRITICAL(&mux);
  strlcpy(t, title, sizeof(t));
  strlcpy(x, text, sizeof(x));
  portEXIT_CRITICAL(&mux);
  uint32_t c = face::moodColor((face::Mood)mood);
  for (auto e : eye) { lv_obj_set_style_bg_color(e, lv_color_hex(c), 0); lv_obj_set_style_shadow_color(e, lv_color_hex(c), 0); }
  lv_obj_set_style_bg_color(icon, lv_color_hex(c), 0);
  uic::setText(lTitle, t);
  uic::setText(lText, x);
  shownMs = millis();
}
void onHide() {}
}  // namespace ui_notify
