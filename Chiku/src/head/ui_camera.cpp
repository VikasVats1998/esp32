// ui_camera.cpp - ESP32-CAM status + QR code of the stream URL.
#include "ui_screens.h"
#include "ui_common.h"
#include "camlink.h"

namespace ui_camera {
static lv_obj_t *qr, *lStatus, *lUrl, *bSleep, *bLed;
static char shownUrl[48] = "";
static bool ledOn = false;

static void onSleep(lv_event_t*) {
  if (!strcmp(camlink::camState(), "on")) camlink::sleepCam(); else camlink::wake();
}
static void onLed(lv_event_t*) { ledOn = !ledOn; camlink::led(ledOn); }

lv_obj_t* create(lv_obj_t* parent) {
  lv_obj_t* p = uic::panel(parent);
  lStatus = uic::label(p, &lv_font_montserrat_20, 0xFFFFFF, "Camera", 50);
  qr = lv_qrcode_create(p);
  lv_qrcode_set_size(qr, 190);
  lv_qrcode_set_dark_color(qr, lv_color_black());
  lv_qrcode_set_light_color(qr, lv_color_white());
  lv_obj_align(qr, LV_ALIGN_TOP_MID, 0, 85);
  lv_obj_set_style_border_color(qr, lv_color_white(), 0);
  lv_obj_set_style_border_width(qr, 6, 0);
  lUrl = uic::label(p, &lv_font_montserrat_14, 0x80C0FF, "", 295);
  bSleep = uic::button(p, "Wake", 98, 335, 120, 44, onSleep);
  bLed = uic::button(p, "Light", 248, 335, 120, 44, onLed);
  return p;
}

void update() {
  char url[48];
  camlink::streamUrl(url, sizeof(url));
  bool alive = camlink::alive();
  char buf[48];
  snprintf(buf, sizeof(buf), alive ? "Camera: %s" : "Camera offline", camlink::camState());
  uic::setText(lStatus, buf);
  if (alive && camlink::streamReady()) {
    if (strcmp(url, shownUrl)) {
      strlcpy(shownUrl, url, sizeof(shownUrl));
      lv_qrcode_update(qr, shownUrl, strlen(shownUrl));
    }
    uic::setText(lUrl, shownUrl);
  } else {
    uic::setText(lUrl, "waiting for cam IP...");
  }
  uic::setButtonText(bSleep, !strcmp(camlink::camState(), "on") ? "Sleep" : "Wake");
}
}  // namespace ui_camera
