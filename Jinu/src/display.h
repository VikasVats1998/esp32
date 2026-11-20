// display.h - ST7789 172x320 (landscape) with a full RAM canvas, so every frame
// is drawn off-screen and pushed in one go (no flicker, no tearing).
// Backlight is PWM; 0 = off.
#pragma once
#include <Arduino_GFX_Library.h>

namespace display {
bool init();
Arduino_GFX* canvas();          // draw here, then call flush()
void flush();
void setBrightness(uint8_t v);  // 0..255
uint8_t brightness();
}  // namespace display
