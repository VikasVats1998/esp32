// display.h - CO5300 AMOLED driver (ESP-IDF esp_lcd QSPI + DMA) + LVGL 9 port, with free screen
// rotation (any angle). LVGL draws one full frame in PSRAM (direct mode); the flush turns every changed
// area by the screen rotation (rotate.h) and sends it in DMA strips with even/odd aligned windows.
// Must be called from the UI task (core 1) only, except setBrightness().
#pragma once
#include <Arduino.h>
#include <lvgl.h>

namespace display {
bool init();                         // starts panel, LVGL, tick
void setBrightness(uint8_t value);   // 0..255 (AMOLED panel command)
uint8_t brightness();
void setRotation(int deg);           // 0..359, picture turned clockwise; redraws everything
int rotation();
void toLogical(int16_t& x, int16_t& y);   // panel touch point -> LVGL point (same rotation)
uint32_t flushCount();               // frames sent to the panel (debug)
void panelOff();                     // sleep the panel (not used by default)
}  // namespace display
