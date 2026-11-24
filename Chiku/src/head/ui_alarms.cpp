// ui_alarms.cpp - alarm list (UI kit): time, days, on/off switch. Add or edit times on chiku.local or
// in the app (the + button says so). Shows every alarm that is on or was changed from the default.
#include "ui_screens.h"
#include "ui_common.h"
#include "ui.h"
#include "storage.h"
#include "events.h"

namespace ui_alarms {
static lv_obj_t *lst, *lEmpty;

static void daysText(uint8_t d, char* out, size_t n) {
  static const char* D[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
  if (d == 0x7F) { strlcpy(out, "Every day", n); return; }
  if (d == 0x1F) { strlcpy(out, "Mon\xE2\x80\x93" "Fri", n); return; }
  if (d == 0x60) { strlcpy(out, "Sat, Sun", n); return; }
  out[0] = 0;
  for (int i = 0; i < 7; i++) if (d & (1 << i)) { if (out[0]) strlcat(out, " ", n); strlcat(out, D[i], n); }
}

static void onSwitch(lv_event_t* e) {
  int i = (int)(intptr_t)lv_event_get_user_data(e);
  lv_obj_t* sw = (lv_obj_t*)lv_event_get_target(e);
  storage::get().alarms[i].enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
  storage::save();
  events::publish(events::EV_SETTINGS);
  lv_obj_t* row = lv_obj_get_parent(sw);
  uic::setColor(lv_obj_get_child(row, 0), storage::get().alarms[i].enabled ? uic::C_TEXT : uic::C_DIM);
}
static void onAdd(lv_event_t*) { ui::toast("Add alarms on chiku.local or in the app"); }

static void rebuild() {
  lv_obj_clean(lst);
  Settings& s = storage::get();
  int shown = 0;
  for (int i = 0; i < MAX_ALARMS; i++) {
    const AlarmCfg& a = s.alarms[i];
    bool isDefault = a.hour == 7 && a.minute == 0 && a.days == 0x7F;
    if (!a.enabled && isDefault) continue;
    shown++;
    lv_obj_t* row = uic::card(lst, 0, 0, 320, 78);
    char buf[24];
    snprintf(buf, sizeof(buf), "%02u:%02u", a.hour, a.minute);
    lv_obj_t* t = uic::label(row, &inter_28, a.enabled ? uic::C_TEXT : uic::C_DIM, buf, 10, LV_ALIGN_TOP_LEFT, 20);
    (void)t;
    daysText(a.days, buf, sizeof(buf));
    uic::label(row, &inter_14, uic::C_MUTED, buf, 48, LV_ALIGN_TOP_LEFT, 20);
    lv_obj_t* sw = lv_switch_create(row);
    lv_obj_set_size(sw, 60, 32);
    lv_obj_align(sw, LV_ALIGN_RIGHT_MID, -18, 0);
    lv_obj_set_style_bg_color(sw, lv_color_hex(0x2A3038), 0);
    lv_obj_set_style_bg_color(sw, lv_color_hex(uic::C_CYAN), (lv_style_selector_t)LV_PART_INDICATOR | LV_STATE_CHECKED);
    if (a.enabled) lv_obj_add_state(sw, LV_STATE_CHECKED);
    lv_obj_add_event_cb(sw, onSwitch, LV_EVENT_VALUE_CHANGED, (void*)(intptr_t)i);
  }
  uic::show(lEmpty, shown == 0);
}

lv_obj_t* create(lv_obj_t* parent) {
  lv_obj_t* p = uic::panel(parent);
  uic::title(p, "Alarms", 58);
  lst = uic::list(p, 96, 230);
  lEmpty = uic::label(p, &inter_16, uic::C_MUTED, "No alarms yet", 190);
  uic::label(p, &inter_14, uic::C_MUTED, "Set times on chiku.local or in the app", 334);
  lv_obj_t* add = uic::circleButton(p, LV_SYMBOL_PLUS, 233, 385, 52, 0x000000, onAdd, nullptr, &inter_20);
  lv_obj_set_style_bg_color(add, lv_color_hex(uic::C_CYAN), 0);
  lv_obj_set_style_border_width(add, 0, 0);
  return p;
}
void update(uint32_t) {}
void onShow() { rebuild(); }
void onHide() {}
}  // namespace ui_alarms
