// display.cpp - see display.h
#include "display.h"
#include "config.h"
#include "rotate.h"
#include <Arduino_GFX_Library.h>
#include "esp_heap_caps.h"

namespace display {

static Arduino_DataBus* qspi = nullptr;
static Arduino_CO5300* gfx = nullptr;
static uint8_t curBrightness = 0;

static uint16_t* frame = nullptr;                 // full logical frame, LVGL draws here
static uint16_t* strip = nullptr;                 // panel lines on their way to the panel
static const int STRIP_LINES = 24;                // even: keeps CO5300 even/odd alignment
static rot::Turn turn = rot::make(0, LCD_W);

// LVGL finished an area of the frame: turn it and send the matching panel area.
static void flushCb(lv_display_t* disp, const lv_area_t* area, uint8_t*) {
  int x1, y1, x2, y2;
  rot::panelArea(turn, LCD_W, area->x1, area->y1, area->x2, area->y2, x1, y1, x2, y2);
  int w = x2 - x1 + 1;
  for (int y = y1; y <= y2; y += STRIP_LINES) {
    int n = min(STRIP_LINES, y2 - y + 1);
    for (int r = 0; r < n; r++) rot::fillRow(turn, frame, LCD_W, y + r, x1, x2, strip + r * w);
    gfx->draw16bitRGBBitmap(x1, y, strip, w, n);
  }
  lv_display_flush_ready(disp);
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

  size_t bytes = LCD_W * LCD_H * 2;
  frame = (uint16_t*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  strip = (uint16_t*)heap_caps_malloc(LCD_W * STRIP_LINES * 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
  if (!frame || !strip) return false;
  memset(frame, 0, bytes);

  lv_display_t* disp = lv_display_create(LCD_W, LCD_H);
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
  lv_display_set_buffers(disp, frame, nullptr, bytes, LV_DISPLAY_RENDER_MODE_DIRECT);
  lv_display_set_flush_cb(disp, flushCb);
  lv_obj_set_style_bg_color(lv_screen_active(), lv_color_black(), 0);
  return true;
}

void setRotation(int deg) {
  turn = rot::make(deg, LCD_W);
  if (frame) {
    gfx->fillScreen(0x0000);
    lv_obj_invalidate(lv_screen_active());     // redraw (and re-send) everything at the new angle
    lv_obj_invalidate(lv_layer_top());
  }
}

int rotation() { return turn.deg; }

void toLogical(int16_t& x, int16_t& y) {
  float lx, ly;
  rot::toLogical(turn, x, y, lx, ly);
  x = (int16_t)constrain(lroundf(lx), 0, LCD_W - 1);
  y = (int16_t)constrain(lroundf(ly), 0, LCD_H - 1);
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
