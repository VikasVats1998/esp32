// display.h - CO5300 AMOLED driver + LVGL 9 port, with free screen rotation (any angle).
// LVGL draws into one full frame in PSRAM (direct mode). For every changed area the flush turns the
// pixels by the screen rotation (rotate.h) and sends that panel area over QSPI in short strips.
// Must be called from the UI task (core 1) only, except setBrightness().
#pragma once
#include <Arduino.h>
#include <lvgl.h>

namespace display {
bool init();                         // starts panel, LVGL, tick
void setBrightness(uint8_t value);   // 0..255 (AMOLED panel command)
uint8_t brightness();
void panelOff();                     // sleep the panel (not used by default)
void setRotation(int deg);           // 0..359, picture turned clockwise; redraws everything
int rotation();
void toLogical(int16_t& x, int16_t& y);   // panel touch point -> LVGL point (same rotation)
}  // namespace display
