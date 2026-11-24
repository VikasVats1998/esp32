// lv_conf.h - minimal LVGL 9 configuration for Chiku (466x466 round AMOLED).
#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 16
#define LV_USE_STDLIB_MALLOC LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_STRING LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_BUILTIN
// LVGL's own heap. All screens are created at boot, so it needs room: 512 KB, placed in PSRAM on
// the robot (on the PC simulator it is a plain static array).
#define LV_MEM_SIZE (512 * 1024U)
#ifdef ESP_PLATFORM
#define LV_MEM_POOL_INCLUDE <esp_heap_caps.h>
#define LV_MEM_POOL_ALLOC(size) heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
#endif
#define LV_MEM_POOL_EXPAND_SIZE 0
#define LV_DEF_REFR_PERIOD 33
#define LV_DPI_DEF 130
#define LV_USE_OS LV_OS_NONE
#define LV_DRAW_SW_SUPPORT_RGB565 1
#define LV_DRAW_SW_DRAW_UNIT_CNT 1
#define LV_USE_DRAW_SW 1
#define LV_USE_LOG 0
#define LV_USE_ASSERT_NULL 1
#define LV_USE_ASSERT_MALLOC 1

#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_48 1
#define LV_FONT_DEFAULT &lv_font_montserrat_20

#define LV_USE_LABEL 1
#define LV_USE_ARC 1
#define LV_USE_BAR 1
#define LV_USE_BUTTON 1
#define LV_USE_IMAGE 1
#define LV_USE_LINE 1
#define LV_USE_SLIDER 1
#define LV_USE_SWITCH 1
#define LV_USE_CANVAS 1
#define LV_USE_QRCODE 1
#define LV_USE_LIST 1
#define LV_USE_ROLLER 1
#define LV_USE_SCALE 1
#define LV_USE_TABLE 0
#define LV_USE_MSGBOX 0
#define LV_USE_TEXTAREA 0
#define LV_USE_KEYBOARD 0
#define LV_USE_DROPDOWN 0
#define LV_USE_CHART 0
#define LV_USE_SPAN 0
#define LV_USE_TABVIEW 0
#define LV_USE_TILEVIEW 0
#define LV_USE_WIN 0
#define LV_USE_MENU 0
#define LV_USE_SPINBOX 0
#define LV_USE_LED 0
#define LV_USE_CALENDAR 0
#define LV_USE_ANIMIMG 0
#define LV_USE_BUTTONMATRIX 1
#define LV_USE_CHECKBOX 0

#define LV_USE_FLEX 1
#define LV_USE_GRID 0
#endif
