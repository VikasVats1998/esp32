// ui_setup.cpp - first-time setup (UI kit, dots at the bottom show the step):
//   1 Welcome (tap Start)   2 Wi-Fi (QR code to join the setup hotspot)   3 Screen orientation
//   4 Servos (calibration)   5 Mic check (say "Hey Chiku" 3x: tests the microphones)   6 Done.
// Steps 3 and 4 open their own screens (ui_orient / ui_calib in setup mode), which come back here.
// Saved to NVS when done; Settings -> First-time setup runs it again.
#include "ui_screens.h"
#include "ui_common.h"
#include "ui.h"
#include "net.h"
#include "storage.h"
#include "mic.h"
#include "face.h"
#include <math.h>

namespace ui_setup {
enum Step { WELCOME, WIFI, SCREEN, SERVOS, MIC, DONE, N_STEPS };
static Step step = WELCOME;
static lv_obj_t *pages[N_STEPS], *dotsRow, *qr, *lWifi, *lHeard, *bars[7];
static uint32_t t = 0, micBase = 0;
static bool waitingSub = false;

static void go(Step s) {
  step = s; t = 0;
  for (int i = 0; i < N_STEPS; i++) uic::show(pages[i], i == s);
  uic::setDots(dotsRow, s + 1);
  if (s == SCREEN) { waitingSub = true; ui_orient::setSetupMode(true); ui::show(ui::S_ORIENT); }
  if (s == SERVOS) { waitingSub = true; ui_calib::setSetupMode(true); ui::show(ui::S_CALIB); }
  if (s == MIC) micBase = mic::bursts();
}
static void onNext(lv_event_t*) { if (step < DONE) go((Step)(step + 1)); }
static void finish() {
  storage::get().setupDone = true;
  storage::save();
  ui::show(ui::S_FACE);
  face::playWakeUp();
}
static void onFinish(lv_event_t*) { finish(); }

static lv_obj_t* page(lv_obj_t* p) { lv_obj_t* o = uic::panel(p); lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0); return o; }
static void arcEye(lv_obj_t* p, int x, int y, uint32_t c) {
  lv_obj_t* a = lv_arc_create(p);
  lv_obj_set_size(a, 96, 96);
  lv_obj_set_pos(a, x, y);
  lv_obj_remove_style(a, nullptr, LV_PART_KNOB);
  lv_obj_remove_flag(a, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_arc_opa(a, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_arc_width(a, 22, LV_PART_INDICATOR);
  lv_obj_set_style_arc_rounded(a, false, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(a, lv_color_hex(c), LV_PART_INDICATOR);
  lv_arc_set_bg_angles(a, 0, 360);
  lv_arc_set_angles(a, 200, 340);
}
// Yellow star (done page), drawn with triangles.
static void drawStars(lv_event_t* e) {
  lv_layer_t* layer = lv_event_get_layer(e);
  for (int s = 0; s < 2; s++) {
    float cx = s ? 316 : 150, cy = 150, R = 56, ri = 26;
    lv_draw_rect_dsc_t c;
    lv_draw_rect_dsc_init(&c);
    c.bg_color = lv_color_hex(0xFFE135);
    c.radius = LV_RADIUS_CIRCLE;
    lv_area_t a = {(int32_t)(cx - ri), (int32_t)(cy - ri), (int32_t)(cx + ri), (int32_t)(cy + ri)};
    lv_draw_rect(layer, &c, &a);
    for (int i = 0; i < 5; i++) {
      float an = (-90 + i * 72) * DEG_TO_RAD, al = an - 36 * DEG_TO_RAD, ar = an + 36 * DEG_TO_RAD;
      lv_draw_triangle_dsc_t d;
      lv_draw_triangle_dsc_init(&d);
      d.bg_color = c.bg_color;
      d.p[0] = {(int32_t)(cx + R * cosf(an)), (int32_t)(cy + R * sinf(an))};
      d.p[1] = {(int32_t)(cx + ri * cosf(al)), (int32_t)(cy + ri * sinf(al))};
      d.p[2] = {(int32_t)(cx + ri * cosf(ar)), (int32_t)(cy + ri * sinf(ar))};
      lv_draw_triangle(layer, &d);
    }
  }
}

lv_obj_t* create(lv_obj_t* parent) {
  lv_obj_t* p = uic::panel(parent);
  // 1 welcome
  lv_obj_t* w = pages[WELCOME] = page(p);
  arcEye(w, 101, 110, uic::C_CYAN);
  arcEye(w, 269, 110, uic::C_CYAN);
  uic::label(w, &inter_28, uic::C_TEXT, "Hi! I'm Chiku", 226);
  uic::label(w, &inter_16, uic::C_MUTED, "Let's get set up", 268);
  uic::button(w, "Start", 173, 304, 120, 46, onNext, nullptr, uic::C_CYAN, 0x000000);
  // 2 wifi
  lv_obj_t* f = pages[WIFI] = page(p);
  uic::title(f, "Connect Wi-Fi", 50);
  qr = lv_qrcode_create(f);
  lv_qrcode_set_size(qr, 150);
  lv_qrcode_set_dark_color(qr, lv_color_black());
  lv_qrcode_set_light_color(qr, lv_color_white());
  lv_obj_set_style_border_color(qr, lv_color_white(), 0);
  lv_obj_set_style_border_width(qr, 6, 0);
  lv_obj_align(qr, LV_ALIGN_TOP_MID, 0, 86);
  static const char WIFI_QR[] = "WIFI:T:nopass;S:" AP_SSID ";;";
  lv_qrcode_update(qr, WIFI_QR, strlen(WIFI_QR));
  lWifi = uic::label(f, &inter_14, uic::C_MUTED, "Scan, or join hotspot", 256);
  uic::label(f, &inter_20, uic::C_CYAN, AP_SSID, 278);
  uic::label(f, &inter_14, uic::C_MUTED, "then open 192.168.4.1", 306);
  uic::button(f, "Skip", 183, 336, 100, 36, onNext, nullptr, 0x000000, uic::C_MUTED, &inter_14);
  pages[SCREEN] = page(p);
  pages[SERVOS] = page(p);
  // 5 mic check
  lv_obj_t* m = pages[MIC] = page(p);
  for (int i = 0; i < 2; i++) {
    lv_obj_t* e = uic::card(m, i ? 266 : 90, 70, 110, 140, nullptr, nullptr, 0x00E5FF, 30);
    (void)e;
  }
  uic::label(m, &inter_14, uic::C_MUTED, "Say", 236);
  uic::label(m, &inter_28, uic::C_TEXT, "\"Hey Chiku\"", 256);
  for (int i = 0; i < 7; i++) {
    bars[i] = uic::plain(m);
    lv_obj_set_size(bars[i], 12, 20);
    lv_obj_set_style_radius(bars[i], 6, 0);
    lv_obj_set_style_bg_opa(bars[i], LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(bars[i], lv_color_hex(uic::C_GREEN), 0);
    lv_obj_align(bars[i], LV_ALIGN_TOP_MID, (i - 3) * 22, 314);
  }
  lHeard = uic::label(m, &inter_14, uic::C_GREEN, "", 352);
  uic::button(m, "Skip", 193, 372, 80, 30, onNext, nullptr, 0x000000, uic::C_MUTED, &inter_14);
  // 6 done
  lv_obj_t* d = pages[DONE] = page(p);
  lv_obj_add_event_cb(d, drawStars, LV_EVENT_DRAW_MAIN_END, nullptr);
  lv_obj_add_flag(d, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(d, onFinish, LV_EVENT_CLICKED, nullptr);
  uic::label(d, &inter_28, uic::C_TEXT, "All set!", 238);
  uic::label(d, &inter_14, uic::C_GREEN, "Saved to memory", 280);
  dotsRow = uic::dots(p, N_STEPS, 404);
  return p;
}

void update(uint32_t dt) {
  t += dt;
  if (step == WIFI) {
    if (net::connected()) {
      char b[64];
      snprintf(b, sizeof(b), LV_SYMBOL_OK " Connected to %s", storage::get().ssid);
      uic::setText(lWifi, b);
      uic::setColor(lWifi, uic::C_GREEN);
      static uint32_t okSince = 0;
      if (!okSince) okSince = t;
      if (t - okSince > 1500 || t < okSince) { okSince = 0; go(SCREEN); }
    }
  } else if (step == MIC) {
    float lv = mic::online() ? mic::level() / 100.f : 0;
    for (int i = 0; i < 7; i++) {
      float shape = 1.f - fabsf(i - 3) / 4.f;
      lv_obj_set_height(bars[i], 10 + (int)(40 * lv * shape * (0.7f + 0.3f * sinf(t / 60.f + i))));
      lv_obj_align(bars[i], LV_ALIGN_TOP_MID, (i - 3) * 22, 314 - lv_obj_get_height(bars[i]) / 2 + 10);
    }
    uint32_t heard = mic::bursts() - micBase;
    char b[40];
    if (!mic::online()) strlcpy(b, "Microphone not found", sizeof(b));
    else snprintf(b, sizeof(b), "%lu / 3 heard%s", (unsigned long)min<uint32_t>(heard, 3), heard >= 3 ? " " LV_SYMBOL_OK : "");
    uic::setText(lHeard, b);
    if (heard >= 3 && t > 600) go(DONE);
  } else if (step == DONE && t > 4000) finish();
}

void onShow() {
  if (waitingSub) {                         // back from orientation / calibration
    waitingSub = false;
    go((Step)(step + 1));
    return;
  }
  go(WELCOME);
}
void onHide() {}
}  // namespace ui_setup
