// ui_weather.cpp - weather screen (data from weather.cpp).
#include "ui_screens.h"
#include "ui_common.h"
#include "weather.h"
#include "storage.h"
#include "net.h"

namespace ui_weather {
static lv_obj_t *lTemp, *lText, *lMinMax, *lInfo;

lv_obj_t* create(lv_obj_t* parent) {
  lv_obj_t* p = uic::panel(parent);
  lTemp = uic::label(p, &lv_font_montserrat_48, 0xFFFFFF, "--", 120);
  lText = uic::label(p, &lv_font_montserrat_28, 0x5CE1FF, "No data yet", 195);
  lMinMax = uic::label(p, &lv_font_montserrat_20, 0xC0C0C0, "", 255);
  lInfo = uic::label(p, &lv_font_montserrat_14, 0x707070, "", 310);
  return p;
}

void update() {
  const WeatherData& w = weather::data();
  char buf[64];
  if (!w.valid) {
    uic::setText(lTemp, "--");
    uic::setText(lText, net::connected() ? "Loading..." : "No Wi-Fi");
    uic::setText(lMinMax, "");
    return;
  }
  snprintf(buf, sizeof(buf), "%.1f C", w.temp);
  uic::setText(lTemp, buf);
  uic::setText(lText, weather::describe(w.code));
  snprintf(buf, sizeof(buf), "Min %.0f   Max %.0f C", w.tmin, w.tmax);
  uic::setText(lMinMax, buf);
  Settings& s = storage::get();
  snprintf(buf, sizeof(buf), "%.2f, %.2f   updated %lu min ago", s.lat, s.lon,
           (unsigned long)((millis() - w.updatedMs) / 60000UL));
  uic::setText(lInfo, buf);
}
}  // namespace ui_weather
