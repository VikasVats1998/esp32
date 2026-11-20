// display.cpp - see display.h
#include "display.h"
#include "config.h"

namespace display {
static Arduino_DataBus* bus = nullptr;
static Arduino_GFX* panel = nullptr;
static Arduino_Canvas* cv = nullptr;
static uint8_t bright = 0;

bool init() {
  bus = new Arduino_ESP32SPI(PIN_LCD_DC, PIN_LCD_CS, PIN_LCD_SCLK, PIN_LCD_MOSI, GFX_NOT_DEFINED);
  panel = new Arduino_ST7789(bus, PIN_LCD_RST, 1 /*landscape*/, true /*IPS*/, 172, 320, 34, 0, 34, 0);
  cv = new Arduino_Canvas(LCD_W, LCD_H, panel);
  if (!cv->begin()) return false;
  cv->fillScreen(0);
  cv->flush();
  ledcAttach(PIN_LCD_BL, 5000, 8);
  setBrightness(0);                       // stay dark until the first frame
  return true;
}

Arduino_GFX* canvas() { return cv; }
void flush() { cv->flush(); }
void setBrightness(uint8_t v) { bright = v; ledcWrite(PIN_LCD_BL, v); }
uint8_t brightness() { return bright; }
}  // namespace display
