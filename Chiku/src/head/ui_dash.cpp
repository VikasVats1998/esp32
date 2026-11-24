// ui_dash.cpp - dashboard screens driven by the hub server (srvlink):
//   ui_light   dimmer ring for the first light      ui_devices  clock + 4 device tiles
//   ui_fan     PWM ring for the first fan            ui_system   health + switches
//   ui_motion  bubble level + gyroscope (works without a server)
// Round 466x466 layout in the UI kit style (true black, cards, Inter), buttons instead of drags so
// swipes still change screen. Light, Home (devices) and Fan are in the swipe carousel.
#include "ui_screens.h"
#include "ui_common.h"
#include "ui.h"
#include "srvlink.h"
#include "imu.h"
#include "power.h"
#include "rtc_time.h"
#include "audio.h"
#include "storage.h"
#include "net.h"
#include "mic.h"
#include "wake.h"
#include <math.h>

namespace {
const uint32_t BG = uic::C_BG, TILE = uic::C_CARD, TILE_ON = 0x0A3A48, LINE = uic::C_LINE;   // UI kit colours
const uint32_t CYAN = uic::C_CYAN, ORANGE = uic::C_ORANGE, GREEN = uic::C_GREEN, GREY = uic::C_MUTED, RED = uic::C_RED;
const int CX = LCD_W / 2;

lv_obj_t* panelDark(lv_obj_t* parent) {
  lv_obj_t* p = uic::panel(parent);
  lv_obj_set_style_bg_color(p, lv_color_hex(BG), 0);
  return p;
}

// 270 degree ring, opening at the bottom, value 0..100
lv_obj_t* ring(lv_obj_t* parent, int size, int width, uint32_t color, uint32_t track) {
  lv_obj_t* a = lv_arc_create(parent);
  lv_obj_set_size(a, size, size);
  lv_obj_center(a);
  lv_obj_remove_style(a, nullptr, LV_PART_KNOB);
  lv_obj_remove_flag(a, LV_OBJ_FLAG_CLICKABLE);
  lv_arc_set_bg_angles(a, 135, 45);
  lv_arc_set_range(a, 0, 100);
  lv_obj_set_style_arc_width(a, width, LV_PART_MAIN);
  lv_obj_set_style_arc_width(a, width, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(a, lv_color_hex(track), LV_PART_MAIN);
  lv_obj_set_style_arc_color(a, lv_color_hex(color), LV_PART_INDICATOR);
  lv_obj_set_style_arc_rounded(a, true, LV_PART_MAIN);
  lv_obj_set_style_arc_rounded(a, true, LV_PART_INDICATOR);
  return a;
}

lv_obj_t* disc(lv_obj_t* parent, int size, uint32_t fill, uint32_t border, int bw = 2) {
  lv_obj_t* o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_set_size(o, size, size);
  lv_obj_center(o);
  lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(o, lv_color_hex(fill), 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(o, lv_color_hex(border), 0);
  lv_obj_set_style_border_width(o, bw, 0);
  return o;
}

lv_obj_t* pill(lv_obj_t* parent, const char* text, uint32_t color, int y) {
  lv_obj_t* l = uic::label(parent, &inter_14, color, text, y);
  lv_obj_set_style_bg_color(l, lv_color_hex(TILE), 0);
  lv_obj_set_style_bg_opa(l, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(l, 12, 0);
  lv_obj_set_style_pad_hor(l, 12, 0);
  lv_obj_set_style_pad_ver(l, 5, 0);
  return l;
}

lv_obj_t* roundBtn(lv_obj_t* parent, const char* text, int x, int y, int w, int h, lv_event_cb_t cb, void* u = nullptr) {
  lv_obj_t* b = uic::button(parent, text, x, y, w, h, cb, u, TILE);
  lv_obj_set_style_border_color(b, lv_color_hex(LINE), 0);
  lv_obj_set_style_border_width(b, 1, 0);
  lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
  return b;
}

void footer(lv_obj_t* l) {                      // "server: linked" style status line
  if (!srvlink::enabled()) uic::setText(l, "no server set (web: /api/set serverUrl)");
  else if (srvlink::linked()) uic::setText(l, LV_SYMBOL_OK " SYNCED");
  else uic::setText(l, LV_SYMBOL_WARNING " server offline");
}

// ---- generic "one device" ring screen (light and fan are the same idea) ----
struct RingScreen {
  srvlink::Type type;
  uint32_t color, track;
  lv_obj_t *arc, *centre, *state, *big, *sub, *top, *foot;
};

void step(RingScreen* r, int delta) {
  int i = srvlink::find(r->type);
  srvlink::Device d;
  if (!srvlink::get(i, d)) return;
  srvlink::setLevel(i, d.level + delta);
}
void toggleDev(RingScreen* r) { srvlink::toggle(srvlink::find(r->type)); }

void updateRing(RingScreen* r, const char* none) {
  int i = srvlink::find(r->type);
  srvlink::Device d;
  if (!srvlink::get(i, d)) {
    lv_arc_set_value(r->arc, 0);
    uic::setText(r->state, "--");
    uic::setText(r->big, "--");
    uic::setText(r->sub, none);
    uic::setText(r->top, "");
    footer(r->foot);
    return;
  }
  char b[48];
  lv_arc_set_value(r->arc, d.on ? d.level : 0);
  lv_obj_set_style_arc_color(r->arc, lv_color_hex(d.on ? r->color : GREY), LV_PART_INDICATOR);
  uic::setText(r->state, d.on ? "ACTIVE" : "OFF");
  snprintf(b, sizeof(b), "%d%%", d.on ? d.level : 0);
  uic::setText(r->big, b);
  if (d.load > 0) snprintf(b, sizeof(b), "%.1fW LOAD", d.load); else strcpy(b, d.name);
  uic::setText(r->top, b);
  footer(r->foot);
}

RingScreen L = {srvlink::T_LIGHT, ORANGE, 0x2A2118};
RingScreen F = {srvlink::T_FAN, CYAN, 0x0B2A33};

void bigButtons(lv_obj_t* p, RingScreen* r, int y) {
  roundBtn(p, LV_SYMBOL_MINUS, CX - 80, y, 64, 56, [](lv_event_t* e) { step((RingScreen*)lv_event_get_user_data(e), -10); }, r);
  roundBtn(p, LV_SYMBOL_PLUS, CX + 16, y, 64, 56, [](lv_event_t* e) { step((RingScreen*)lv_event_get_user_data(e), 10); }, r);
}

void ringCommon(lv_obj_t* p, RingScreen* r, uint32_t centreBorder) {
  r->arc = ring(p, 440, 20, r->color, r->track);
  r->centre = disc(p, 150, 0x10151C, centreBorder, 3);
  lv_obj_align(r->centre, LV_ALIGN_CENTER, 0, -62);
  lv_obj_add_flag(r->centre, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(r->centre, [](lv_event_t* e) { toggleDev((RingScreen*)lv_event_get_user_data(e)); }, LV_EVENT_CLICKED, r);
  r->state = uic::label(r->centre, &inter_20, r->color, "--", 0, LV_ALIGN_CENTER);
  r->big = uic::label(p, &inter_44, 0xFFFFFF, "--", 238);
  r->sub = uic::label(p, &inter_14, r->color, "", 295);
  r->top = pill(p, "", GREEN, 16);
  r->foot = uic::label(p, &inter_14, GREY, "", 412);
}
}  // namespace

// ================= LIGHT =================
namespace ui_light {
lv_obj_t* create(lv_obj_t* parent) {
  lv_obj_t* p = panelDark(parent);
  ringCommon(p, &L, 0x3A2E1E);
  bigButtons(p, &L, 335);
  return p;
}
void update(uint32_t) {
  updateRing(&L, "no light on the server");
  int i = srvlink::find(srvlink::T_LIGHT);
  srvlink::Device d;
  if (srvlink::get(i, d) && d.temp > 0) {
    char b[40];
    snprintf(b, sizeof(b), "%dK %s GLOW", d.temp, d.temp < 3500 ? "WARM" : d.temp < 5000 ? "NEUTRAL" : "COLD");
    uic::setText(L.sub, b);
  }
}
void onShow() {}
void onHide() {}
}  // namespace ui_light

// ================= FAN =================
namespace ui_fan {
static void preset(lv_event_t* e) {
  int i = srvlink::find(srvlink::T_FAN);
  srvlink::setLevel(i, (int)(intptr_t)lv_event_get_user_data(e));
}
lv_obj_t* create(lv_obj_t* parent) {
  lv_obj_t* p = panelDark(parent);
  ringCommon(p, &F, 0x0E5260);
  static const int V[4] = {25, 50, 75, 100};
  static const char* N[4] = {"25%", "50%", "75%", "MAX"};
  for (int k = 0; k < 4; k++) {
    lv_obj_t* b = uic::button(p, N[k], 78 + k * 78, 345, 70, 40, preset, (void*)(intptr_t)V[k], TILE);
    lv_obj_set_style_border_color(b, lv_color_hex(LINE), 0);
    lv_obj_set_style_border_width(b, 1, 0);
  }
  roundBtn(p, LV_SYMBOL_MINUS, 14, 180, 64, 64, [](lv_event_t*) { step(&F, -5); });
  roundBtn(p, LV_SYMBOL_PLUS, LCD_W - 78, 180, 64, 64, [](lv_event_t*) { step(&F, 5); });
  return p;
}
void update(uint32_t) {
  updateRing(&F, "no fan on the server");
  srvlink::Device d;
  if (srvlink::get(srvlink::find(srvlink::T_FAN), d)) uic::setText(F.sub, "PWM FREQ: 25 kHz");
}
void onShow() {}
void onHide() {}
}  // namespace ui_fan

// ================= DEVICES + CLOCK =================
namespace ui_devices {
static lv_obj_t *tile[4], *tileName[4], *tileState[4], *lTime, *lEnv, *lFoot;
static void onTile(lv_event_t* e) { srvlink::toggle((int)(intptr_t)lv_event_get_user_data(e)); }

lv_obj_t* create(lv_obj_t* parent) {
  lv_obj_t* p = panelDark(parent);
  lv_obj_t* r = disc(p, 214, 0x10151C, CYAN, 3);
  (void)r;
  lTime = uic::label(p, &inter_44, 0xFFFFFF, "--:--", 0, LV_ALIGN_CENTER);
  lEnv = pill(p, "", CYAN, 160);
  lFoot = uic::label(p, &inter_14, GREY, "", 412);
  const int X[4] = {18, 18, LCD_W - 18 - 132, LCD_W - 18 - 132};
  const int Y[4] = {130, 262, 130, 262};
  for (int k = 0; k < 4; k++) {
    tile[k] = uic::button(p, "", X[k], Y[k], 132, 74, onTile, (void*)(intptr_t)k, TILE);
    lv_obj_set_style_border_color(tile[k], lv_color_hex(LINE), 0);
    lv_obj_set_style_border_width(tile[k], 1, 0);
    lv_obj_del(lv_obj_get_child(tile[k], 0));
    tileName[k] = uic::label(tile[k], &inter_14, 0xFFFFFF, "", 8, LV_ALIGN_TOP_MID);
    tileState[k] = uic::label(tile[k], &inter_14, CYAN, "", -8, LV_ALIGN_BOTTOM_MID);
  }
  return p;
}
void update(uint32_t) {
  struct tm t;
  char b[48];
  if (rtc_time::localTime(t)) { snprintf(b, sizeof(b), "%02d:%02d", t.tm_hour, t.tm_min); uic::setText(lTime, b); }
  float te = srvlink::envTemp();
  int hu = srvlink::envHumidity();
  if (!isnan(te)) snprintf(b, sizeof(b), "%.1f C  %d%% RH", te, hu < 0 ? 0 : hu); else b[0] = 0;
  uic::setText(lEnv, b);
  for (int k = 0; k < 4; k++) {
    srvlink::Device d;
    bool have = srvlink::get(k, d);
    lv_obj_set_style_opa(tile[k], have ? LV_OPA_COVER : LV_OPA_30, 0);
    if (!have) { uic::setText(tileName[k], "--"); uic::setText(tileState[k], ""); continue; }
    uic::setText(tileName[k], d.name);
    if (d.type == srvlink::T_CLIMATE) snprintf(b, sizeof(b), d.on ? "ON  %d C" : "OFF", d.level);
    else if (d.type == srvlink::T_SWITCH) snprintf(b, sizeof(b), d.on ? "ON" : "OFF");
    else snprintf(b, sizeof(b), d.on ? "ON  %d%%" : "OFF", d.level);
    uic::setText(tileState[k], b);
    lv_obj_set_style_bg_color(tile[k], lv_color_hex(d.on ? TILE_ON : TILE), 0);
  }
  footer(lFoot);
}
void onShow() {}
void onHide() {}
}  // namespace ui_devices

// ================= SYSTEM =================
namespace ui_system {
static lv_obj_t *arc, *lCore, *lHeap, *lWifi, *lBat, *barMic, *lMic, *swClap, *swWake, *swSound, *lFoot;

static void onClap(lv_event_t* e) {
  storage::get().clapWake = lv_obj_has_state(lv_event_get_target_obj(e), LV_STATE_CHECKED);
  storage::save();
}
static void onWake(lv_event_t* e) {
  storage::get().wakeWord = lv_obj_has_state(lv_event_get_target_obj(e), LV_STATE_CHECKED);
  storage::save();                                    // takes effect after a restart
}
static void onSound(lv_event_t* e) { audio::setMuted(!lv_obj_has_state(lv_event_get_target_obj(e), LV_STATE_CHECKED)); }

static lv_obj_t* switchRow(lv_obj_t* p, const char* title, const char* sub, int y, lv_event_cb_t cb) {
  lv_obj_t* row = lv_obj_create(p);
  lv_obj_remove_style_all(row);
  lv_obj_set_size(row, 300, 46);
  lv_obj_align(row, LV_ALIGN_TOP_MID, 0, y);
  lv_obj_set_style_bg_color(row, lv_color_hex(TILE), 0);
  lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(row, 14, 0);
  lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t* a = uic::label(row, &inter_14, 0xFFFFFF, title, 4, LV_ALIGN_TOP_LEFT, 14);
  lv_obj_set_style_text_align(a, LV_TEXT_ALIGN_LEFT, 0);
  lv_obj_t* b = uic::label(row, &inter_14, GREY, sub, -4, LV_ALIGN_BOTTOM_LEFT, 14);
  lv_obj_set_style_text_align(b, LV_TEXT_ALIGN_LEFT, 0);
  lv_obj_t* sw = lv_switch_create(row);
  lv_obj_align(sw, LV_ALIGN_RIGHT_MID, -10, 0);
  lv_obj_set_style_bg_color(sw, lv_color_hex(GREEN), (lv_style_selector_t)LV_PART_INDICATOR | LV_STATE_CHECKED);
  lv_obj_add_event_cb(sw, cb, LV_EVENT_VALUE_CHANGED, nullptr);
  return sw;
}

static lv_obj_t* btnOff = nullptr;
static uint32_t offArmedAt = 0;
// Two taps within 3 s: the first arms ("SURE?"), the second powers the robot off.
static void onPowerOff(lv_event_t*) {
  uint32_t now = millis();
  if (offArmedAt && now - offArmedAt < 3000) { ui::show(ui::S_GOODNIGHT); return; }   // good night, then off
  offArmedAt = now;
  uic::setButtonText(btnOff, "SURE?");
  lv_obj_set_style_bg_color(btnOff, lv_color_hex(0xD83A3A), 0);
}

lv_obj_t* create(lv_obj_t* parent) {
  lv_obj_t* p = panelDark(parent);
  arc = ring(p, 450, 10, GREEN, 0x12261C);
  lBat = uic::label(p, &inter_14, GREEN, "BAT", 40);
  lCore = pill(p, "", CYAN, 60);
  lHeap = pill(p, "", ORANGE, 88);
  lWifi = uic::label(p, &inter_14, GREY, "", 117);
  lMic = uic::label(p, &inter_14, CYAN, "MIC", 136);
  barMic = lv_bar_create(p);
  lv_obj_set_size(barMic, 220, 8);
  lv_obj_align(barMic, LV_ALIGN_TOP_MID, 0, 156);
  lv_bar_set_range(barMic, 0, 100);
  lv_obj_set_style_bg_color(barMic, lv_color_hex(0x262C36), LV_PART_MAIN);
  lv_obj_set_style_bg_color(barMic, lv_color_hex(CYAN), LV_PART_INDICATOR);
  swClap = switchRow(p, "Clap wake", "clap once / twice", 176, onClap);
  swWake = switchRow(p, "Voice wake word", "say \"Hi ESP\" (restart)", 228, onWake);
  swSound = switchRow(p, "Speaker", "sound on/off", 280, onSound);
  uic::button(p, "REBOOT", 65, 336, 104, 40, [](lv_event_t*) { ESP.restart(); }, nullptr, TILE);
  uic::button(p, "SYNC", 181, 336, 104, 40, [](lv_event_t*) { srvlink::syncNow(); }, nullptr, TILE);
  btnOff = uic::button(p, LV_SYMBOL_POWER " OFF", 297, 336, 104, 40, onPowerOff, nullptr, 0x6B2222);
  lFoot = uic::label(p, &inter_14, GREY, "", 388);
  return p;
}
static void setSw(lv_obj_t* sw, bool on) {
  if (on) lv_obj_add_state(sw, LV_STATE_CHECKED); else lv_obj_remove_state(sw, LV_STATE_CHECKED);
}
void update(uint32_t) {
  if (offArmedAt && millis() - offArmedAt >= 3000) {      // not confirmed: disarm
    offArmedAt = 0;
    uic::setButtonText(btnOff, LV_SYMBOL_POWER " OFF");
    lv_obj_set_style_bg_color(btnOff, lv_color_hex(0x6B2222), 0);
  }
  char b[72];
  int pct = power::batteryPercent();
  lv_arc_set_value(arc, pct < 0 ? 0 : pct);
  snprintf(b, sizeof(b), "BAT %d%%  %.2fV  %s%s", pct < 0 ? 0 : pct, power::batteryVolts(),
           power::charging() ? LV_SYMBOL_CHARGE " " : "", power::vbusVolts() > 3.5f ? "USB" : "");
  uic::setText(lBat, b);
  snprintf(b, sizeof(b), "CORE %.1f C", power::coreTempC());
  uic::setText(lCore, b);
  snprintf(b, sizeof(b), "HEAP %d KB  PSRAM %d KB", (int)(ESP.getFreeHeap() / 1024), (int)(ESP.getFreePsram() / 1024));
  uic::setText(lHeap, b);
  snprintf(b, sizeof(b), "WiFi %d dBm  %s", net::rssi(), net::ip());
  uic::setText(lWifi, b);
  snprintf(b, sizeof(b), "MIC %d%%  %s", mic::level(), wake::running() ? "wake word on" : mic::online() ? "listening" : "mic offline");
  uic::setText(lMic, b);
  lv_bar_set_value(barMic, mic::level(), LV_ANIM_OFF);
  setSw(swClap, storage::get().clapWake);
  setSw(swWake, storage::get().wakeWord);
  setSw(swSound, !audio::muted());
  footer(lFoot);
}
void onShow() {}
void onHide() {}
}  // namespace ui_system

// ================= MOTION (gyroscope / accelerometer) =================
namespace ui_motion {
static lv_obj_t *bubble, *lTilt, *lAcc, *lGyr, *lCal;
static const int R = 130;                       // bubble travel radius in px

lv_obj_t* create(lv_obj_t* parent) {
  lv_obj_t* p = panelDark(parent);
  disc(p, 2 * R + 40, 0x10151C, CYAN, 3);
  disc(p, R, 0x10151C, LINE, 1);
  disc(p, 30, 0x10151C, LINE, 1);
  lv_obj_t* h = lv_obj_create(p);                // crosshair
  lv_obj_remove_style_all(h);
  lv_obj_set_size(h, 2 * R + 40, 1);
  lv_obj_center(h);
  lv_obj_set_style_bg_color(h, lv_color_hex(LINE), 0);
  lv_obj_set_style_bg_opa(h, LV_OPA_COVER, 0);
  lv_obj_t* v = lv_obj_create(p);
  lv_obj_remove_style_all(v);
  lv_obj_set_size(v, 1, 2 * R + 40);
  lv_obj_center(v);
  lv_obj_set_style_bg_color(v, lv_color_hex(LINE), 0);
  lv_obj_set_style_bg_opa(v, LV_OPA_COVER, 0);
  bubble = disc(p, 34, GREEN, 0xFFFFFF, 2);
  lTilt = pill(p, "", CYAN, 18);
  lAcc = uic::label(p, &inter_14, GREY, "", 372);
  lGyr = uic::label(p, &inter_14, ORANGE, "", 394);
  lCal = uic::label(p, &inter_14, GREY, "", 416);
  lv_obj_t* cal = uic::button(p, "CALIBRATE", CX - 70, 66, 140, 36, [](lv_event_t*) { imu::calibrate(); }, nullptr, TILE);
  lv_obj_set_style_text_font(lv_obj_get_child(cal, 0), &inter_14, 0);
  return p;
}
void update(uint32_t) {                                  // called every frame while visible
  float ax, ay, az, gx, gy, gz;
  imu::getAccel(ax, ay, az);
  imu::getGyro(gx, gy, gz);
  float px = constrain(imu::roll() / 45.f, -1.f, 1.f) * R, py = constrain(imu::pitch() / 45.f, -1.f, 1.f) * R;   // fused angles
  float len = sqrtf(px * px + py * py);
  if (len > R) { px *= R / len; py *= R / len; }
  lv_obj_align(bubble, LV_ALIGN_CENTER, (int)px, (int)-py);
  bool level = len < 8;
  lv_obj_set_style_bg_color(bubble, lv_color_hex(level ? GREEN : ORANGE), 0);
  static uint32_t last = 0;
  if (millis() - last < 200) return;
  last = millis();
  char b[64];
  if (!imu::online()) { uic::setText(lTilt, "IMU offline"); return; }
  snprintf(b, sizeof(b), "TILT %d deg%s", (int)imu::tiltDeg(), imu::flipped() ? "  UPSIDE DOWN" : "");
  uic::setText(lTilt, b);
  snprintf(b, sizeof(b), "ACC  %+.2f  %+.2f  %+.2f g", ax, ay, az);
  uic::setText(lAcc, b);
  snprintf(b, sizeof(b), "GYRO %+.0f  %+.0f  %+.0f dps", gx, gy, gz);
  uic::setText(lGyr, b);
  uic::setText(lCal, imu::calibrated() ? "gyro calibrated" : "calibrating - keep still");
}
void onShow() {}
void onHide() {}
}  // namespace ui_motion
