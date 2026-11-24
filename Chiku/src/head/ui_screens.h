// ui_screens.h - every screen module has create(parent) and update(dtMs) (every frame while it is
// visible); some also have onShow() / onHide(). ui.cpp lists them in one table.
#pragma once
#include <lvgl.h>
#include <stdint.h>

#define UI_SCREEN(ns) namespace ns { lv_obj_t* create(lv_obj_t* parent); void update(uint32_t dt); void onShow(); void onHide(); }
UI_SCREEN(ui_clock)
UI_SCREEN(ui_weather)
UI_SCREEN(ui_timer)
UI_SCREEN(ui_menu)
UI_SCREEN(ui_alarms)
UI_SCREEN(ui_moves)
UI_SCREEN(ui_sensors)
UI_SCREEN(ui_sonar)
UI_SCREEN(ui_camera)
UI_SCREEN(ui_network)
UI_SCREEN(ui_games)
UI_SCREEN(ui_desk)
UI_SCREEN(ui_settings)
UI_SCREEN(ui_calib)
UI_SCREEN(ui_orient)
UI_SCREEN(ui_power)
UI_SCREEN(ui_goodnight)
UI_SCREEN(ui_ringing)
UI_SCREEN(ui_notify)
UI_SCREEN(ui_boot)
UI_SCREEN(ui_setup)
#undef UI_SCREEN

// Extra entry points used by other modules
namespace ui_calib { void setSetupMode(bool on); bool finished(); }
namespace ui_orient { void setSetupMode(bool on); bool finished(); }
namespace ui_notify { void post(const char* title, const char* text, uint8_t mood); }   // any task
namespace ui_games { void onPadOrShake(int kind); }   // 1 = pad touched, 2 = shake
