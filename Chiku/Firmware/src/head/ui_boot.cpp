// ui_boot.cpp - boot screen (UI kit): logo + progress ring while Wi-Fi connects (at most
// BOOT_MAX_MS). Then the first-time setup (first start) or the face with the wake-up animation.
#include "ui_screens.h"
#include "ui_common.h"
#include "ui.h"
#include "net.h"
#include "storage.h"
#include "face.h"

namespace ui_boot {
static lv_obj_t *ring, *lPct;
static uint32_t t = 0;
static float pct = 0;

lv_obj_t* create(lv_obj_t* parent) {
  lv_obj_t* p = uic::panel(parent);
  ring = uic::ring(p, 430, 14, uic::C_CYAN);
  uic::label(p, &inter_44, uic::C_TEXT, BOOT_TITLE, 176);
  lv_obj_t* s = uic::label(p, &inter_16, uic::C_CYAN, BOOT_SUBTITLE, 234);
  lv_obj_set_style_text_letter_space(s, 3, 0);
  lPct = uic::label(p, &inter_16, uic::C_MUTED, "Booting 0%", 286);
  return p;
}

void update(uint32_t dt) {
  t += dt;
  bool haveWifi = storage::get().ssid[0] != 0;
  // target: 40 % when the hardware is up, then creep towards 95 % while Wi-Fi connects
  float target = t < 800 ? 40.f * t / 800 : 40.f + 55.f * min<uint32_t>(t - 800, BOOT_MAX_MS) / BOOT_MAX_MS;
  if (net::connected() || !haveWifi || t > BOOT_MAX_MS) target = 100;
  pct += (target - pct) * 0.2f;
  if (target == 100 && pct > 99) pct = 100;
  char b[32];
  snprintf(b, sizeof(b), "%s %d%%", t > 800 && haveWifi && !net::connected() ? "Connecting Wi-Fi" : "Booting", (int)pct);
  uic::setText(lPct, b);
  uic::setRing(ring, (int)(pct * 10));
  if (pct >= 100 && t > 1200) {
    if (!storage::get().setupDone) ui::show(ui::S_SETUP);
    else { ui::show(ui::S_FACE); face::playWakeUp(); }
  }
}
void onShow() { t = 0; pct = 0; }
void onHide() {}
}  // namespace ui_boot
