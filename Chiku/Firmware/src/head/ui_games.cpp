// ui_games.cpp - games (UI kit "Games"), all on Chiku itself:
//   Reaction: wait until the eyes flash, then tap the screen or a touch pad as fast as you can (5 rounds).
//   Dice: shake Chiku (or tap) to roll.
// The camera games of the kit (rock-paper-scissors, Simon) need image recognition: not included.
#include "ui_screens.h"
#include "ui_common.h"
#include "ui.h"
#include "audio.h"
#include <math.h>

namespace ui_games {
enum Mode { HUB, REACTION, DICE };
static Mode mode = HUB;
static lv_obj_t *hub, *react, *dice, *lBest;
// reaction
enum RState { R_WAIT, R_GO, R_RESULT, R_DONE };
static RState rs = R_WAIT;
static lv_obj_t *rEye[2], *rBig, *rMs, *rInfo, *rRing;
static uint32_t rT = 0, rWaitMs = 0, best = 0, sum = 0;
static int round_ = 0;
static volatile int pending = 0;          // input from pads / shake (other task)
// dice
static lv_obj_t *die, *pips[7], *dInfo;
static uint32_t rollLeft = 0, rollTick = 0;
static int face_ = 6;

static lv_obj_t* screenOf(Mode m) { return m == HUB ? hub : m == REACTION ? react : dice; }
static void setMode(Mode m) { mode = m; for (Mode x : {HUB, REACTION, DICE}) uic::show(screenOf(x), x == m); }

// ---------- reaction ----------
static void eyesColor(uint32_t c) { for (auto e : rEye) { lv_obj_set_style_bg_color(e, lv_color_hex(c), 0); lv_obj_set_style_shadow_color(e, lv_color_hex(c), 0); } }
static void nextRound() {
  rs = R_WAIT; rT = 0; rWaitMs = random(1500, 4000);
  eyesColor(uic::C_YELLOW);
  uic::setText(rBig, "Wait...");
  uic::setColor(rBig, uic::C_MUTED);
  uic::setText(rMs, "");
}
static void startReaction() { round_ = 0; sum = 0; setMode(REACTION); nextRound(); }
static void reactionTap() {
  char b[40];
  switch (rs) {
    case R_WAIT:                                       // too early
      uic::setText(rBig, "Too soon!");
      uic::setColor(rBig, uic::C_RED);
      audio::beep(300, 120);
      rs = R_RESULT; rT = 0;
      break;
    case R_GO: {
      uint32_t ms = rT;
      round_++; sum += ms;
      if (!best || ms < best) best = ms;
      snprintf(b, sizeof(b), "%lu ms", (unsigned long)ms);
      uic::setText(rMs, b);
      uic::setText(rBig, ms < 250 ? "Wow!" : "Got it");
      uic::setColor(rBig, uic::C_YELLOW);
      eyesColor(uic::C_YELLOW);
      audio::play(audio::MEL_CLICK);
      rs = round_ >= 5 ? R_DONE : R_RESULT; rT = 0;
      if (rs == R_DONE) {
        snprintf(b, sizeof(b), "avg %lu ms", (unsigned long)(sum / 5));
        uic::setText(rBig, b);
        audio::play(audio::MEL_HAPPY);
      }
      break;
    }
    case R_DONE: startReaction(); break;
    default: break;
  }
}
static void updateReaction(uint32_t dt) {
  rT += dt;
  if (rs == R_WAIT && rT >= rWaitMs) {
    rs = R_GO; rT = 0;
    eyesColor(0xFFFFFF);
    uic::setText(rBig, "TAP!");
    uic::setColor(rBig, uic::C_YELLOW);
  } else if (rs == R_RESULT && rT > 1500) nextRound();
  char b[48];
  snprintf(b, sizeof(b), "Best %lu ms \xC2\xB7 %d / 5", (unsigned long)best, round_);
  uic::setText(rInfo, best ? b : "Tap when my eyes flash");
  uic::setRing(rRing, round_ * 200);
}

// ---------- dice ----------
static void showDie(int n) {
  static const uint8_t ON[7][7] = {   // which of the 7 pips (TL, TR, ML, C, MR, BL, BR) are lit
    {0}, {0,0,0,1,0,0,0}, {1,0,0,0,0,0,1}, {1,0,0,1,0,0,1}, {1,1,0,0,0,1,1}, {1,1,0,1,0,1,1}, {1,1,1,0,1,1,1}};
  for (int i = 0; i < 7; i++) uic::show(pips[i], ON[n][i]);
}
static void roll() { rollLeft = 700; rollTick = 0; audio::beep(900, 30); }
static void updateDice(uint32_t dt) {
  if (!rollLeft) return;
  rollLeft = rollLeft > dt ? rollLeft - dt : 0;
  rollTick += dt;
  if (rollTick > 70 || !rollLeft) {
    rollTick = 0;
    face_ = random(1, 7);
    showDie(face_);
    lv_obj_set_style_transform_rotation(die, rollLeft ? random(-150, 150) : 0, 0);
    if (!rollLeft) { audio::play(face_ == 6 ? audio::MEL_HAPPY : audio::MEL_CLICK); }
  }
}

// ---------- input ----------
void onPadOrShake(int kind) { pending = kind; }
static void onPress(lv_event_t*) { if (mode == REACTION) reactionTap(); else if (mode == DICE) roll(); }
static void onTile(lv_event_t* e) { if (lv_event_get_user_data(e)) { setMode(DICE); showDie(face_); } else startReaction(); }

static lv_obj_t* tile(lv_obj_t* p, int x, const char* icon, const char* name, uint32_t c, void* user) {
  lv_obj_t* t = uic::card(p, x, 110, 150, 150, onTile, user);
  lv_obj_set_style_border_width(t, 3, 0);
  lv_obj_set_style_border_color(t, lv_color_hex(c), 0);
  lv_obj_set_style_border_opa(t, LV_OPA_40, 0);
  uic::label(t, &inter_28, c, icon, 34);
  uic::label(t, &inter_16, uic::C_TEXT, name, 90);
  return t;
}

lv_obj_t* create(lv_obj_t* parent) {
  lv_obj_t* p = uic::panel(parent);
  hub = uic::panel(p);
  uic::title(hub, "Games", 58);
  tile(hub, 73, LV_SYMBOL_CHARGE, "Reaction", uic::C_YELLOW, nullptr);
  tile(hub, 243, LV_SYMBOL_SHUFFLE, "Dice", uic::C_GREEN, (void*)1);
  uic::label(hub, &inter_16, uic::C_MUTED, "\"Chiku, let's play!\"", 290);
  lv_obj_t* bc = uic::chip(hub, "", 324, uic::C_YELLOW, uic::C_CARD, &inter_16);
  lBest = uic::chipLabel(bc);

  react = uic::panel(p);
  lv_obj_add_flag(react, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(react, onPress, LV_EVENT_PRESSED, nullptr);
  rRing = uic::ring(react, 430, 10, uic::C_YELLOW);
  for (int i = 0; i < 2; i++) {
    rEye[i] = uic::plain(react);
    lv_obj_set_size(rEye[i], 64, 64);
    lv_obj_set_pos(rEye[i], i ? 263 : 139, 96);
    lv_obj_set_style_radius(rEye[i], LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(rEye[i], LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(rEye[i], 18, 0);
    lv_obj_set_style_shadow_opa(rEye[i], 130, 0);
  }
  rBig = uic::label(react, &inter_44, uic::C_YELLOW, "", 190);
  rMs = uic::label(react, &inter_20, uic::C_TEXT, "", 254);
  rInfo = uic::label(react, &inter_14, uic::C_MUTED, "", 300);

  dice = uic::panel(p);
  lv_obj_add_flag(dice, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(dice, onPress, LV_EVENT_PRESSED, nullptr);
  die = uic::card(dice, 158, 108, 150, 150, nullptr, nullptr, 0xF2F4F7, 28);
  lv_obj_set_style_transform_pivot_x(die, 75, 0);
  lv_obj_set_style_transform_pivot_y(die, 75, 0);
  static const int PX[7] = {38, 112, 38, 75, 112, 38, 112}, PY[7] = {38, 38, 75, 75, 75, 112, 112};
  for (int i = 0; i < 7; i++) {
    pips[i] = uic::plain(die);
    lv_obj_set_size(pips[i], 24, 24);
    lv_obj_set_pos(pips[i], PX[i] - 12, PY[i] - 12);
    lv_obj_set_style_radius(pips[i], LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(pips[i], LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(pips[i], lv_color_hex(0x111418), 0);
  }
  dInfo = uic::label(dice, &inter_16, uic::C_MUTED, "Shake me or tap to roll", 296);
  setMode(HUB);
  return p;
}

void update(uint32_t dt) {
  int pd = pending;
  if (pd) {
    pending = 0;
    if (mode == REACTION && pd == 1) reactionTap();
    if (mode == DICE) roll();
  }
  if (mode == REACTION) updateReaction(dt);
  else if (mode == DICE) updateDice(dt);
  else {
    char b[32];
    if (best) snprintf(b, sizeof(b), "Best: %lu ms", (unsigned long)best); else strlcpy(b, "Play a round!", sizeof(b));
    uic::setText(lBest, b);
  }
}
void onShow() { setMode(HUB); }
void onHide() { setMode(HUB); }
}  // namespace ui_games
