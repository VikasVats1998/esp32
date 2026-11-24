// ui_menu.cpp - radial menu (UI kit, swipe up from the face): 8 coloured rings around "Menu".
// Tap a ring to open it, tap "Menu" or swipe down to go back to the face.
#include "ui_screens.h"
#include "ui_common.h"
#include "ui.h"
#include <math.h>

namespace ui_menu {
struct Item { const char* text; uint32_t color; ui::Screen screen; };
static const Item ITEMS[8] = {             // clockwise from the top
  {"Home", uic::C_GREEN, ui::S_DEVICES},      {"Alarms", uic::C_CYAN, ui::S_ALARMS},
  {"Timer", uic::C_ORANGE, ui::S_TIMER},   {"Sensors", uic::C_PINK, ui::S_SENSORS},
  {"Games", uic::C_YELLOW, ui::S_GAMES},   {"Moves", uic::C_PURPLE, ui::S_MOVES},
  {"Settings", uic::C_GREY, ui::S_SETTINGS}, {"Power", uic::C_RED, ui::S_POWER},
};

static void onItem(lv_event_t* e) { ui::show(((const Item*)lv_event_get_user_data(e))->screen); }
static void onCentre(lv_event_t*) { ui::show(ui::S_FACE); }

lv_obj_t* create(lv_obj_t* parent) {
  lv_obj_t* p = uic::panel(parent);
  for (int i = 0; i < 8; i++) {
    float a = (i * 45 - 90) * DEG_TO_RAD;
    uic::circleButton(p, ITEMS[i].text, 233 + (int)(158 * cosf(a)), 233 + (int)(158 * sinf(a)), 80,
                      ITEMS[i].color, onItem, (void*)&ITEMS[i]);
  }
  lv_obj_t* c = uic::button(p, "Menu", 173, 208, 120, 50, onCentre, nullptr, 0x000000, uic::C_TEXT, &inter_28);
  (void)c;
  return p;
}
void update(uint32_t) {}
void onShow() {}
void onHide() {}
}  // namespace ui_menu
