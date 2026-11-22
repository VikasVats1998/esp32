// ui_screens.h - every screen has: create(parent) and update() (about 4 Hz,
// only called while visible) and optionally onShow().
#pragma once
#include <lvgl.h>

namespace ui_clock    { lv_obj_t* create(lv_obj_t* parent); void update(); }
namespace ui_weather  { lv_obj_t* create(lv_obj_t* parent); void update(); }
namespace ui_timer    { lv_obj_t* create(lv_obj_t* parent); void update(); }
namespace ui_sonar    { lv_obj_t* create(lv_obj_t* parent); void update(); }
namespace ui_camera   { lv_obj_t* create(lv_obj_t* parent); void update(); }
namespace ui_settings { lv_obj_t* create(lv_obj_t* parent); void update(); }
namespace ui_menu     { lv_obj_t* create(lv_obj_t* parent); void update(); void onShow(); }
namespace ui_desk     { lv_obj_t* create(lv_obj_t* parent); void update(); }
namespace ui_calib    { lv_obj_t* create(lv_obj_t* parent); void update(); void onShow(); void onHide(); }
namespace ui_light    { lv_obj_t* create(lv_obj_t* parent); void update(); }
namespace ui_devices  { lv_obj_t* create(lv_obj_t* parent); void update(); }
namespace ui_fan      { lv_obj_t* create(lv_obj_t* parent); void update(); }
namespace ui_system   { lv_obj_t* create(lv_obj_t* parent); void update(); }
namespace ui_motion   { lv_obj_t* create(lv_obj_t* parent); void update(); }   // every frame
