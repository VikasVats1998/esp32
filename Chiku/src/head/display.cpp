// display.cpp - see display.h
#include "display.h"
#include "config.h"
#include <Arduino_GFX_Library.h>
#include "esp_heap_caps.h"

namespace display {

static Arduino_DataBus* qspi = nullptr;
static Arduino_CO5300* gfx = nullptr;
static uint8_t curBrightness = 0;

static const uint32_t BUF_LINES = 60;                 // lines per partial buffer
static const uint32_t BUF_PIXELS = LCD_W * BUF_LINES;

// LVGL asks us to send a finished area to the panel.
static void flushCb(lv_display_t* disp, const lv_area_t* area, uint8_t* px) {
  uint32_t w = lv_area_get_width(area);
  uint32_t h = lv_area_get_height(area);
  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t*)px, w, h);
  lv_display_flush_ready(disp);
}

// CO5300 needs even start / odd end coordinates. Called whenever LVGL
// invalidates an area, so every later flush is already aligned.
static void roundAreaCb(lv_event_t* e) {
  lv_area_t* a = (lv_area_t*)lv_event_get_param(e);
  a->x1 &= ~1;
  a->y1 &= ~1;
  a->x2 |= 1;
  a->y2 |= 1;
}

bool init() {
  qspi = new Arduino_ESP32QSPI(PIN_LCD_CS, PIN_LCD_SCLK, PIN_LCD_SDIO0, PIN_LCD_SDIO1,
                               PIN_LCD_SDIO2, PIN_LCD_SDIO3);
  gfx = new Arduino_CO5300(qspi, PIN_LCD_RST, 0, LCD_W, LCD_H, 6, 0, 0, 0);
  if (!gfx->begin()) return false;
  gfx->fillScreen(0x0000);
  setBrightness(0);                      // stay dark until the first frame is drawn

  lv_init();
  lv_tick_set_cb([]() -> uint32_t { return millis(); });

  uint8_t* b1 = (uint8_t*)heap_caps_malloc(BUF_PIXELS * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  uint8_t* b2 = (uint8_t*)heap_caps_malloc(BUF_PIXELS * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!b1 || !b2) return false;

  lv_display_t* disp = lv_display_create(LCD_W, LCD_H);
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
  lv_display_set_buffers(disp, b1, b2, BUF_PIXELS * 2, LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(disp, flushCb);
  lv_display_add_event_cb(disp, roundAreaCb, LV_EVENT_INVALIDATE_AREA, nullptr);
  lv_obj_set_style_bg_color(lv_screen_active(), lv_color_black(), 0);
  return true;
}

void setBrightness(uint8_t v) {
  curBrightness = v;
  if (gfx) gfx->setBrightness(v);
}

uint8_t brightness() { return curBrightness; }

void panelOff() {
  if (gfx) gfx->displayOff();
}

}  // namespace display
