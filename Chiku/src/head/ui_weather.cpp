// ui_weather.cpp - weather (UI kit): icon made of shapes, big temperature, description, high / low.
#include "ui_screens.h"
#include "ui_common.h"
#include "weather.h"
#include "net.h"

namespace ui_weather {
static lv_obj_t *sun, *cloud, *drops, *lTemp, *lText, *lHL, *lInfo;
static int shownCode = -999;

static lv_obj_t* disc(lv_obj_t* p, int x, int y, int w, int h, uint32_t c) {
  lv_obj_t* o = uic::plain(p);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, w, h);
  lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(o, lv_color_hex(c), 0);
  return o;
}

lv_obj_t* create(lv_obj_t* parent) {
  lv_obj_t* p = uic::panel(parent);
  sun = disc(p, 170, 72, 74, 74, 0xFFC93C);
  lv_obj_set_style_shadow_width(sun, 24, 0);
  lv_obj_set_style_shadow_color(sun, lv_color_hex(0xFFC93C), 0);
  lv_obj_set_style_shadow_opa(sun, 90, 0);
  cloud = uic::plain(p);                                  // three puffs + base
  lv_obj_set_pos(cloud, 178, 84);
  lv_obj_set_size(cloud, 140, 80);
  disc(cloud, 6, 34, 128, 46, 0xC9D3DF);
  disc(cloud, 20, 16, 56, 56, 0xC9D3DF);
  disc(cloud, 56, 0, 70, 70, 0xC9D3DF);
  drops = uic::label(p, &inter_28, 0x6EA8FF, "/ / /", 160);
  lTemp = uic::label(p, &inter_44, uic::C_TEXT, "--", 196);
  lText = uic::label(p, &inter_14, uic::C_MUTED, "No data yet", 258);
  lHL = uic::label(p, &inter_14, uic::C_CYAN, "", 284);
  lInfo = uic::label(p, &inter_14, uic::C_DIM, "", 330);
  return p;
}

// Which parts of the icon to show for a WMO weather code.
static void setIcon(int code, bool day) {
  bool clear = code <= 1, partly = code == 2, rain = (code >= 51 && code <= 67) || (code >= 80 && code <= 82) || code >= 95;
  bool snow = (code >= 71 && code <= 77) || code == 85 || code == 86;
  uic::show(sun, clear || partly);
  lv_obj_set_style_bg_color(sun, lv_color_hex(day ? 0xFFC93C : 0xD8DEE9), 0);   // moon at night
  lv_obj_set_x(sun, clear ? 196 : 170);
  lv_obj_set_y(sun, clear ? 86 : 72);
  uic::show(cloud, !clear);
  uic::show(drops, rain || snow);
  uic::setText(drops, code >= 95 ? LV_SYMBOL_CHARGE : snow ? "*  *  *" : "/  /  /");
  uic::setColor(drops, code >= 95 ? 0xFFD43B : snow ? 0xFFFFFF : 0x6EA8FF);
}

void update(uint32_t) {
  const WeatherData& w = weather::data();
  char buf[64];
  if (!w.valid) {
    uic::setText(lTemp, "--");
    uic::setText(lText, net::connected() ? "Loading..." : "No Wi-Fi");
    uic::setText(lHL, "");
    if (shownCode != -1) { shownCode = -1; setIcon(3, true); }
    return;
  }
  if (w.code != shownCode) { shownCode = w.code; setIcon(w.code, w.isDay); }
  snprintf(buf, sizeof(buf), "%.0f\xC2\xB0" "C", w.temp);
  uic::setText(lTemp, buf);
  uic::setText(lText, weather::describe(w.code));
  snprintf(buf, sizeof(buf), "H %.0f\xC2\xB0   L %.0f\xC2\xB0", w.tmax, w.tmin);
  uic::setText(lHL, buf);
  snprintf(buf, sizeof(buf), "updated %lu min ago", (unsigned long)((millis() - w.updatedMs) / 60000UL));
  uic::setText(lInfo, buf);
}
void onShow() {}
void onHide() {}
}  // namespace ui_weather
