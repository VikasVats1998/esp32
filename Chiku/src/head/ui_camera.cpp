// ui_camera.cpp - ESP32-CAM (UI kit "ESP32-CAM"): live status chip, QR code of the stream URL
// (open it on the phone), round buttons: snapshot, light, sleep / wake.
#include "ui_screens.h"
#include "ui_common.h"
#include "ui.h"
#include "camlink.h"

namespace ui_camera {
static lv_obj_t *qr, *chip, *lUrl, *bLight, *bSleep;
static char shownUrl[48] = "";
static bool ledOn = false;

static void onSnap(lv_event_t*) { camlink::snap(); ui::toast("Snapshot taken"); }
static void onLight(lv_event_t*) {
  ledOn = !ledOn;
  camlink::led(ledOn);
  lv_obj_set_style_bg_color(bLight, lv_color_hex(ledOn ? uic::C_CYAN : 0x1A1E24), 0);
}
static void onSleep(lv_event_t*) {
  if (!strcmp(camlink::camState(), "on")) camlink::sleepCam(); else camlink::wake();
}

lv_obj_t* create(lv_obj_t* parent) {
  lv_obj_t* p = uic::panel(parent);
  chip = uic::chip(p, "Camera offline", 58, uic::C_TEXT, uic::C_CARD, &inter_16);
  qr = lv_qrcode_create(p);
  lv_qrcode_set_size(qr, 170);
  lv_qrcode_set_dark_color(qr, lv_color_black());
  lv_qrcode_set_light_color(qr, lv_color_white());
  lv_obj_align(qr, LV_ALIGN_TOP_MID, 0, 108);
  lv_obj_set_style_border_color(qr, lv_color_white(), 0);
  lv_obj_set_style_border_width(qr, 6, 0);
  lUrl = uic::label(p, &inter_14, uic::C_MUTED, "", 296);
  uic::circleButton(p, LV_SYMBOL_IMAGE, 163, 360, 56, uic::C_TEXT, onSnap, nullptr, &inter_20);
  bLight = uic::circleButton(p, LV_SYMBOL_EYE_OPEN, 233, 360, 56, uic::C_TEXT, onLight, nullptr, &inter_20);
  bSleep = uic::circleButton(p, LV_SYMBOL_POWER, 303, 360, 56, uic::C_TEXT, onSleep, nullptr, &inter_20);
  for (lv_obj_t* b : {lv_obj_get_child(p, -3), bLight, bSleep}) lv_obj_set_style_border_width(b, 0, 0);
  return p;
}

void update(uint32_t) {
  char url[48], buf[64];
  camlink::streamUrl(url, sizeof(url));
  bool alive = camlink::alive();
  bool on = !strcmp(camlink::camState(), "on");
  if (!alive) strlcpy(buf, "Camera offline", sizeof(buf));
  else if (on) strlcpy(buf, LV_SYMBOL_VIDEO "  LIVE \xC2\xB7 ESP32-CAM", sizeof(buf));
  else strlcpy(buf, "ESP32-CAM asleep", sizeof(buf));
  uic::setText(uic::chipLabel(chip), buf);
  uic::setColor(uic::chipLabel(chip), alive && on ? uic::C_RED : uic::C_MUTED);
  if (alive && camlink::streamReady()) {
    if (strcmp(url, shownUrl)) { strlcpy(shownUrl, url, sizeof(shownUrl)); lv_qrcode_update(qr, shownUrl, strlen(shownUrl)); }
    uic::setText(lUrl, shownUrl);
  } else uic::setText(lUrl, "waiting for the camera...");
  uic::setColor(lv_obj_get_child(bSleep, 0), on ? uic::C_TEXT : uic::C_GREEN);
}
void onShow() {}
void onHide() {}
}  // namespace ui_camera
