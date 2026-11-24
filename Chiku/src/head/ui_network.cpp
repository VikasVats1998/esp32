// ui_network.cpp - network at a glance (UI kit "Network"): IP, Wi-Fi name and signal, and every link:
// web API, camera (UART), standing desk, time (NTP).
#include "ui_screens.h"
#include "ui_common.h"
#include "net.h"
#include "storage.h"
#include "camlink.h"
#include "desk.h"
#include "rtc_time.h"

namespace ui_network {
enum { L_API, L_CAM, L_DESK, L_TIME, L_COUNT };
static lv_obj_t *lIp, *lInfo, *st[L_COUNT];

lv_obj_t* create(lv_obj_t* parent) {
  lv_obj_t* p = uic::panel(parent);
  uic::title(p, "Network", 58);
  uic::label(p, &inter_28, uic::C_GREEN, LV_SYMBOL_WIFI, 92);
  lIp = uic::label(p, &inter_28, uic::C_CYAN, "", 130);
  lInfo = uic::label(p, &inter_14, uic::C_MUTED, "", 170);
  static const char* NAME[L_COUNT] = {"Web API", "Camera", "Desk", "Time"};
  for (int i = 0; i < L_COUNT; i++) {
    lv_obj_t* c = uic::card(p, 78 + (i % 2) * 160, 204 + (i / 2) * 72, 150, 62);
    uic::label(c, &inter_14, uic::C_MUTED, NAME[i], 10, LV_ALIGN_TOP_LEFT, 14);
    st[i] = uic::label(c, &inter_16, uic::C_TEXT, "", 32, LV_ALIGN_TOP_LEFT, 14);
  }
  uic::label(p, &inter_14, uic::C_DIM, ROBOT_NAME ".local", 370);
  return p;
}

static void state(int i, bool ok, const char* text) {
  char b[40];
  snprintf(b, sizeof(b), "\xE2\x80\xA2 %s", text);    // dot
  uic::setText(st[i], b);
  uic::setColor(st[i], ok ? uic::C_GREEN : uic::C_MUTED);
}

void update(uint32_t) {
  char b[64];
  bool on = net::connected();
  uic::setText(lIp, on ? net::ip() : (net::portalActive() ? "192.168.4.1" : "offline"));
  if (on) snprintf(b, sizeof(b), "%s \xC2\xB7 %d dBm \xC2\xB7 DHCP", storage::get().ssid, net::rssi());
  else if (net::portalActive()) snprintf(b, sizeof(b), "Setup hotspot \"%s\"", AP_SSID);
  else strlcpy(b, "not connected", sizeof(b));
  uic::setText(lInfo, b);
  state(L_API, on, on ? "Online" : "Offline");
  state(L_CAM, camlink::alive(), camlink::alive() ? "UART" : "Offline");
  desk::State d = desk::state();
  state(L_DESK, d.paired && d.ok, !d.paired ? "Not paired" : d.ok ? "Linked" : "No link");
  state(L_TIME, rtc_time::valid(), rtc_time::valid() ? "Synced" : "Waiting");
}
void onShow() {}
void onHide() {}
}  // namespace ui_network
