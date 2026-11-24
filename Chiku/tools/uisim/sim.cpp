// sim.cpp - Chiku UI simulator: runs the real face + screen code (src/head) with LVGL on a PC and
// writes every screen and mood as a PNG (like the UI kit pages). Hardware modules are faked in
// stubs.cpp. Also checks the free screen rotation math (rotate.h).
//   bash Chiku/tools/uisim/run.sh            -> PNGs in Chiku/tools/uisim/out/
#include <Arduino.h>
#include <lvgl.h>
#include <sys/stat.h>
#include <vector>
#include "sim.h"
#include "png.h"
#include "config.h"
#include "ui.h"
#include "ui_screens.h"
#include "face.h"
#include "rotate.h"

static std::vector<uint16_t> frame(LCD_W * LCD_H);
static std::string outDir = "out";
static int failures = 0;

static void flushCb(lv_display_t* d, const lv_area_t*, uint8_t*) { lv_display_flush_ready(d); }

static void run(uint32_t ms) {
  for (uint32_t t = 0; t < ms; t += 33) {
    sim.ms += 33;
    ui::update(33);
    lv_timer_handler();
  }
}

// Save the panel as the eye sees it (rotation applied), outside the round glass in light grey.
static void snap(const char* name) {
  lv_refr_now(nullptr);
  rot::Turn t = rot::make(sim.rotation, LCD_W);
  std::vector<uint16_t> row(LCD_W);
  std::vector<uint8_t> rgb(LCD_W * LCD_H * 3);
  for (int y = 0; y < LCD_H; y++) {
    rot::fillRow(t, frame.data(), LCD_W, y, 0, LCD_W - 1, row.data());
    for (int x = 0; x < LCD_W; x++) {
      uint8_t* p = &rgb[(y * LCD_W + x) * 3];
      float dx = x - 232.5f, dy = y - 232.5f;
      float r = sqrtf(dx * dx + dy * dy);
      if (r > 233) { p[0] = 0xE8; p[1] = 0xE9; p[2] = 0xED; continue; }
      if (r > 226) { p[0] = 0x16; p[1] = 0x18; p[2] = 0x1C; continue; }      // bezel
      uint16_t c = row[x];
      p[0] = ((c >> 11) & 31) * 255 / 31; p[1] = ((c >> 5) & 63) * 255 / 63; p[2] = (c & 31) * 255 / 31;
    }
  }
  std::string path = outDir + "/" + name + ".png";
  if (!png::write(path.c_str(), rgb.data(), LCD_W, LCD_H)) { printf("cannot write %s\n", path.c_str()); failures++; }
}

// ---- rotation math checks ----
static void check(bool ok, const char* what) { if (!ok) { printf("FAIL: %s\n", what); failures++; } }
static void rotationTests() {
  for (int deg : {0, 1, 27, 90, 135, 180, 270, 359}) {
    rot::Turn t = rot::make(deg, LCD_W);
    for (float x : {0.f, 100.f, 232.f, 400.f}) for (float y : {10.f, 233.f, 450.f}) {
      float lx, ly, px, py;
      rot::toLogical(t, x, y, lx, ly);
      rot::toPanel(t, lx, ly, px, py);
      check(fabsf(px - x) < 0.05f && fabsf(py - y) < 0.05f, "toLogical/toPanel round trip");
    }
    int x1, y1, x2, y2;
    rot::panelArea(t, LCD_W, 100, 120, 140, 180, x1, y1, x2, y2);
    check((x1 & 1) == 0 && (y1 & 1) == 0 && (x2 & 1) == 1 && (y2 & 1) == 1, "panel area even/odd aligned");
    // every corner of the logical area must land inside the panel area
    for (float cx : {100.f, 141.f}) for (float cy : {120.f, 181.f}) {
      float px, py;
      rot::toPanel(t, cx, cy, px, py);
      check(px >= x1 - 0.5f && px <= x2 + 1.5f && py >= y1 - 0.5f && py <= y2 + 1.5f, "panel area covers the logical area");
    }
  }
  // 90 deg: the logical top-left corner shows at the panel top-right
  rot::Turn t = rot::make(90, LCD_W);
  std::vector<uint16_t> f(LCD_W * LCD_H, 0), row(LCD_W);
  f[0] = 0xFFFF;
  rot::fillRow(t, f.data(), LCD_W, 0, 0, LCD_W - 1, row.data());
  check(row[LCD_W - 1] == 0xFFFF, "90 deg: top-left goes to top-right");
  // 0 deg: plain copy
  rot::Turn z = rot::make(0, LCD_W);
  f[5 * LCD_W + 7] = 0x1234;
  rot::fillRow(z, f.data(), LCD_W, 5, 0, LCD_W - 1, row.data());
  check(row[7] == 0x1234, "0 deg: copy");
  printf("rotation tests: %s\n", failures ? "FAILED" : "pass");
}

int main(int argc, char** argv) {
  if (argc > 1) outDir = argv[1];
  mkdir(outDir.c_str(), 0755);
  srand(7);
  sim.timerOn = false;          // no timer ring over the faces
  rotationTests();

  lv_init();
  lv_tick_set_cb([]() -> uint32_t { return sim.ms; });
  lv_display_t* d = lv_display_create(LCD_W, LCD_H);
  lv_display_set_color_format(d, LV_COLOR_FORMAT_RGB565);
  lv_display_set_buffers(d, frame.data(), nullptr, frame.size() * 2, LV_DISPLAY_RENDER_MODE_DIRECT);
  lv_display_set_flush_cb(d, flushCb);
  lv_obj_set_style_bg_color(lv_screen_active(), lv_color_black(), 0);
  ui::init();

  // boot
  run(500); snap("power_boot");
  run(2500);
  // wake-up animation (120 ms per frame)
  face::playWakeUp();
  for (int i = 1; i <= 5; i++) { run(i == 1 ? 60 : 120); char n[24]; snprintf(n, sizeof(n), "anim_wake_%d", i); snap(n); }
  run(1500);
  // look-around
  const struct { float x, y; const char* n; } LOOK[] = {{-1, 0, "anim_look_left"}, {1, 0, "anim_look_right"}, {0, -1, "anim_look_up"}, {0, 1, "anim_look_down"}};
  for (auto& l : LOOK) { face::lookAt(l.x, l.y); run(400); snap(l.n); }
  face::lookAt(0, 0); run(400);
  // every mood, with its caption like the kit
  face::setCaption(true);
  for (int m = 0; m < face::MOOD_COUNT; m++) {
    face::setMood((face::Mood)m, 0);
    run(900);
    char n[32];
    snprintf(n, sizeof(n), "mood_%02d_%s", m, face::moodName((face::Mood)m));
    snap(n);
  }
  face::setCaption(false);
  sim.speech = 0.8f; face::setMood(face::M_HAPPY, 0); run(400); snap("voice_speaking"); sim.speech = -1;
  face::setMood(face::M_NORMAL, 0);
  // battery looks + sleep
  face::setStatus(face::ST_LOW_BATTERY, 8); run(900); snap("power_low_battery");
  face::setStatus(face::ST_CHARGING, 74); run(900); snap("power_charging");
  face::setStatus(face::ST_NONE, 74);
  face::setSleeping(true); run(900); snap("power_sleep");
  face::setSleeping(false); run(900);
  // free rotation: the same face turned 30 deg (what the panel shows when the setting is 30)
  sim.rotation = 30; snap("rotation_30deg"); sim.rotation = 0;

  // every screen
  sim.timerOn = true;
  for (int s = ui::S_CLOCK; s < ui::S_COUNT; s++) {
    if (s == ui::S_BOOT || s == ui::S_GOODNIGHT) continue;
    if (s == ui::S_RINGING) sim.ringing = 0;
    if (s == ui::S_NOTIFY) ui_notify::post("Doorbell", "Someone's at the door", face::M_SURPRISED);
    ui::show((ui::Screen)s);
    run(700);
    char n[32];
    snprintf(n, sizeof(n), "screen_%s", ui::screenName((ui::Screen)s));
    snap(n);
    sim.ringing = -1;
    ui::show(ui::S_FACE);
    run(100);
  }
  sim.timerOn = false; ui::show(ui::S_TIMER); run(300); snap("screen_timer_idle");
  ui::show(ui::S_GOODNIGHT); run(500); snap("power_goodnight");
  printf("screenshots in %s/\n", outDir.c_str());
  return failures ? 1 : 0;
}
