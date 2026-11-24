// ui.cpp - see ui.h
#include "ui.h"
#include "config.h"
#include "ui_screens.h"
#include "ui_common.h"
#include "face.h"
#include "display.h"
#include "touch.h"
#include "power.h"
#include "storage.h"
#include "alarm_timer.h"

namespace ui {

// Face is not a ui_* module: small wrappers so it fits the table.
static lv_obj_t* faceCreate(lv_obj_t* p) { return face::create(p); }
static void faceUpdate(uint32_t dt) { face::update(dt); }
static void none() {}

struct Def {
  const char* name;
  lv_obj_t* (*create)(lv_obj_t*);
  void (*update)(uint32_t);
  void (*onShow)();
  void (*onHide)();
  Screen back;                     // where "back" (swipe down / right) goes
  touch::GestureMode gestures;
};
#define SCR(name, ns, back, g) {name, ns::create, ns::update, ns::onShow, ns::onHide, back, touch::g}
static const Def DEFS[S_COUNT] = {
  {"face", faceCreate, faceUpdate, none, none, S_FACE, touch::G_ALL},
  SCR("clock",       ui_clock,     S_FACE,     G_ALL),
  SCR("weather",     ui_weather,   S_FACE,     G_ALL),
  SCR("timer",       ui_timer,     S_FACE,     G_ALL),
  SCR("menu",        ui_menu,      S_FACE,     G_ALL),
  SCR("alarms",      ui_alarms,    S_MENU,     G_HORIZONTAL),
  SCR("moves",       ui_moves,     S_MENU,     G_HORIZONTAL),
  SCR("sensors",     ui_sensors,   S_MENU,     G_ALL),
  SCR("sonar",       ui_sonar,     S_SENSORS,  G_ALL),
  SCR("camera",      ui_camera,    S_SENSORS,  G_ALL),
  SCR("network",     ui_network,   S_SENSORS,  G_ALL),
  SCR("games",       ui_games,     S_MENU,     G_ALL),
  SCR("desk",        ui_desk,      S_SETTINGS, G_NONE),
  SCR("settings",    ui_settings,  S_MENU,     G_HORIZONTAL),
  SCR("calibration", ui_calib,     S_SETTINGS, G_NONE),
  SCR("orientation", ui_orient,    S_SETTINGS, G_NONE),
  SCR("power",       ui_power,     S_FACE,     G_ALL),
  SCR("goodnight",   ui_goodnight, S_FACE,     G_NONE),
  SCR("ringing",     ui_ringing,   S_FACE,     G_NONE),
  SCR("notify",      ui_notify,    S_FACE,     G_ALL),
  SCR("boot",        ui_boot,      S_FACE,     G_NONE),
  SCR("setup",       ui_setup,     S_FACE,     G_NONE),
  SCR("light",       ui_light,     S_FACE,     G_ALL),          // hub dashboard (ui_dash.cpp)
  SCR("devices",     ui_devices,   S_FACE,     G_ALL),
  SCR("fan",         ui_fan,       S_FACE,     G_ALL),
  SCR("motion",      ui_motion,    S_SENSORS,  G_ALL),
  SCR("system",      ui_system,    S_SETTINGS, G_ALL),
};
// Swipe carousel (UI kit: Clock <-> Weather <-> Home hub <-> Timer). Other screens: menu / buttons.
static const Screen ORDER[] = {S_FACE, S_CLOCK, S_WEATHER, S_DEVICES, S_LIGHT, S_FAN, S_TIMER};
static const int ORDER_N = sizeof(ORDER) / sizeof(ORDER[0]);
static int orderIndex(Screen s) { for (int i = 0; i < ORDER_N; i++) if (ORDER[i] == s) return i; return -1; }

static lv_obj_t* screens[S_COUNT];
static Screen cur = S_BOOT;
static lv_obj_t* testObj = nullptr;       // full-screen colour for display tests
static volatile int reqPattern = 0;
static volatile int reqRot = -1;
static int shownPattern = 0;
static lv_obj_t* timerRing = nullptr;     // timer progress ring on the top layer
static lv_obj_t* holdRing = nullptr;      // fills while the finger rests (power menu)
static lv_obj_t* toastChip = nullptr;
static uint32_t toastLeft = 0, slowAcc = 0;
static bool demoOn = false;
static uint32_t demoAcc = 0;
static uint8_t demoIdx = 0;

Screen current() { return cur; }
const char* screenName(Screen s) { return s < S_COUNT ? DEFS[s].name : "?"; }
bool screenFromName(const char* n, Screen& out) {
  for (int i = 0; i < S_COUNT; i++) if (!strcmp(n, DEFS[i].name)) { out = (Screen)i; return true; }
  return false;
}

void applyBrightness() {
  uint8_t b = storage::get().brightness;
  switch (power::state()) {
    case power::ACTIVE: display::setBrightness(b); break;
    case power::DIM:    display::setBrightness(min<int>(b, BRIGHTNESS_DIM)); break;
    case power::SLEEP:  display::setBrightness(BRIGHTNESS_SLEEP); break;
  }
}

void show(Screen s) {
  if (s >= S_COUNT || s == cur) return;
  DEFS[cur].onHide();
  for (int i = 0; i < S_COUNT; i++) uic::show(screens[i], i == s);
  cur = s;
  touch::setGestures(DEFS[s].gestures);
  if (s != S_FACE && demoOn) { demoOn = false; face::setCaption(false); }
  DEFS[s].onShow();
  slowAcc = 1000;
}

void back() { show(DEFS[cur].back); }

void toast(const char* text) {
  uic::setText(uic::chipLabel(toastChip), text);
  uic::show(toastChip, true);
  toastLeft = 2500;
}

void init() {
  lv_obj_t* root = lv_screen_active();
  lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);
  for (int i = 0; i < S_COUNT; i++) {
    screens[i] = DEFS[i].create(root);
    lv_obj_add_flag(screens[i], LV_OBJ_FLAG_HIDDEN);
  }
  timerRing = uic::ring(lv_layer_top(), LCD_W - 6, 6, uic::C_ORANGE, 0x000000);
  lv_obj_set_style_arc_opa(timerRing, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_add_flag(timerRing, LV_OBJ_FLAG_HIDDEN);
  holdRing = uic::ring(lv_layer_top(), LCD_W - 24, 10, uic::C_RED, 0x000000);
  lv_obj_set_style_arc_opa(holdRing, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_add_flag(holdRing, LV_OBJ_FLAG_HIDDEN);
  toastChip = uic::chip(lv_layer_top(), "", 372, uic::C_TEXT, uic::C_CARD_HI, &inter_16);
  lv_obj_add_flag(toastChip, LV_OBJ_FLAG_HIDDEN);
  testObj = uic::plain(lv_layer_top());
  lv_obj_set_size(testObj, LCD_W, LCD_H);
  lv_obj_set_style_bg_opa(testObj, LV_OPA_COVER, 0);
  lv_obj_add_flag(testObj, LV_OBJ_FLAG_HIDDEN);
  display::setRotation(storage::get().screenRot);
  cur = S_FACE;                 // anything but S_BOOT, so show() runs
  show(S_BOOT);
}

void setTestPattern(int p) { reqPattern = p; }
void requestRotation(int deg) { reqRot = deg; }

static void applyPattern() {
  int p = reqPattern;
  if (p == shownPattern) return;
  shownPattern = p;
  if (p == 0) { lv_obj_add_flag(testObj, LV_OBJ_FLAG_HIDDEN); return; }
  static const uint32_t C[] = {0, 0xFF0000, 0x00FF00, 0x0000FF, 0xFFFFFF, 0x000000, 0x000000};
  lv_obj_set_style_bg_color(testObj, lv_color_hex(C[p > 6 ? 5 : p]), 0);
  if (p == 6) {                                       // grey ramp: look for banding
    lv_obj_set_style_bg_grad_color(testObj, lv_color_white(), 0);
    lv_obj_set_style_bg_grad_dir(testObj, LV_GRAD_DIR_HOR, 0);
  } else lv_obj_set_style_bg_grad_dir(testObj, LV_GRAD_DIR_NONE, 0);
  lv_obj_remove_flag(testObj, LV_OBJ_FLAG_HIDDEN);
}

void startMoodDemo() {
  show(S_FACE);
  demoOn = true; demoIdx = 0; demoAcc = 1800;
  face::setCaption(true);
}

static void updateRings() {
  int slot = alarm_timer::activeTimer();
  bool ringOk = cur != S_TIMER && cur != S_RINGING && cur != S_LIGHT && cur != S_FAN && cur != S_BOOT && cur != S_GOODNIGHT && cur != S_SETUP;
  if (slot < 0 || !ringOk) lv_obj_add_flag(timerRing, LV_OBJ_FLAG_HIDDEN);
  else {
    uint32_t total = alarm_timer::timerTotalMs(slot), left = alarm_timer::timerRemainingMs(slot);
    uic::setRing(timerRing, total ? (int)(1000ULL * left / total) : 0);     // shrinks as time runs out
    lv_obj_remove_flag(timerRing, LV_OBJ_FLAG_HIDDEN);
  }
}


void update(uint32_t dtMs) {
  applyPattern();
  if (reqRot >= 0) { display::setRotation(reqRot); reqRot = -1; }
  DEFS[cur].update(dtMs);
  // hold to open the power menu: show progress after a short moment
  uint32_t h = (cur == S_FACE) ? touch::holdMs() : 0;
  if (h > 600) {
    uic::setRing(holdRing, (int)min<uint32_t>(1000, (h - 600) * 1000 / (POWER_MENU_HOLD_MS - 600)));
    lv_obj_remove_flag(holdRing, LV_OBJ_FLAG_HIDDEN);
  } else if (!lv_obj_has_flag(holdRing, LV_OBJ_FLAG_HIDDEN)) lv_obj_add_flag(holdRing, LV_OBJ_FLAG_HIDDEN);
  if (toastLeft) {
    toastLeft = toastLeft > dtMs ? toastLeft - dtMs : 0;
    if (!toastLeft) lv_obj_add_flag(toastChip, LV_OBJ_FLAG_HIDDEN);
  }
  slowAcc += dtMs;
  if (slowAcc >= 1000) { slowAcc = 0; updateRings(); }
  if (demoOn) {
    demoAcc += dtMs;
    if (demoAcc > 1800) {
      demoAcc = 0;
      if (demoIdx >= face::MOOD_COUNT) { demoOn = false; face::setCaption(false); face::setMood(face::M_NORMAL, 0); }
      else face::setMood((face::Mood)demoIdx++, 1700);
    }
  }
}

void onEvent(const events::Event& e) {
  int pos = orderIndex(cur);
  bool carousel = pos >= 0;
  switch (e.type) {
    case events::EV_SWIPE:
      if (e.a != 0) {                                   // left / right
        if (carousel) show(ORDER[(pos - e.a + ORDER_N) % ORDER_N]);   // swipe left (-1) = next
        else if (DEFS[cur].gestures == touch::G_HORIZONTAL && e.a > 0) back();
      } else if (e.b < 0) {                             // up
        if (carousel) show(S_MENU);
      } else if (cur != S_FACE) back();                 // down
      break;
    case events::EV_TOUCH_LONG:
      if (cur == S_FACE) show(S_POWER);
      break;
    case events::EV_SCREEN: show((Screen)e.a); break;
    case events::EV_ALARM_RING: if (cur != S_SETUP && cur != S_BOOT) show(S_RINGING); break;
    case events::EV_ALARM_STOP: if (cur == S_RINGING) show(S_FACE); break;
    case events::EV_NOTIFY: if (cur != S_SETUP && cur != S_BOOT && cur != S_RINGING) show(S_NOTIFY); break;
    case events::EV_POWER:
      if (e.a == power::SLEEP && cur != S_RINGING && cur != S_SETUP && cur != S_GOODNIGHT) show(S_FACE);
      applyBrightness();
      break;
    case events::EV_SETTINGS: applyBrightness(); break;
    default: break;
  }
}

}  // namespace ui
