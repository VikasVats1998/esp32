// display.h - CO5300 AMOLED driver + LVGL 9 port.
// Two partial render buffers live in PSRAM. LVGL renders one buffer while the
// other is sent over QSPI. Dirty areas are rounded to even pixels (CO5300 needs
// that). Must be called from the UI task (core 1) only, except setBrightness().
#pragma once
#include <Arduino.h>
#include <lvgl.h>

namespace display {
bool init();                         // starts panel, LVGL, tick
void setBrightness(uint8_t value);   // 0..255 (AMOLED panel command)
uint8_t brightness();
void setRotation(int deg);   // 0/90/180/270, software rotation of the whole UI
int rotation();
uint32_t flushCount();               // frames sent to the panel (debug)
void panelOff();                     // sleep the panel (not used by default)
}  // namespace display
