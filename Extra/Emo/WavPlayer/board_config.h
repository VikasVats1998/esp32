#pragma once

// Select the board you are compiling for.
#define WAVPLAYER_BOARD_NODEMCU 1
#define WAVPLAYER_BOARD_WAVESHARE_C6 2
#define WAVPLAYER_BOARD_WAVESHARE_S3 3

#define WAVPLAYER_BOARD WAVPLAYER_BOARD_NODEMCU

#if WAVPLAYER_BOARD == WAVPLAYER_BOARD_NODEMCU
// Common ESP32 DevKit / NodeMCU pin choices.
#define WAVPLAYER_I2S_BCLK 26
#define WAVPLAYER_I2S_LRC  25
#define WAVPLAYER_I2S_DIN  22
#define WAVPLAYER_BUTTON   0

#elif WAVPLAYER_BOARD == WAVPLAYER_BOARD_WAVESHARE_C6
// Waveshare ESP32-C6-LCD-1.47; GPIO4/5 are also connected to the TF slot.
#define WAVPLAYER_I2S_BCLK 4
#define WAVPLAYER_I2S_LRC  5
#define WAVPLAYER_I2S_DIN  17
#define WAVPLAYER_BUTTON   9

#elif WAVPLAYER_BOARD == WAVPLAYER_BOARD_WAVESHARE_S3
// Example pins for a Waveshare ESP32-S3 board without display/audio conflicts.
// Override all three in your build flags or here for your exact S3 board.
#ifndef WAVPLAYER_S3_I2S_BCLK
#define WAVPLAYER_S3_I2S_BCLK 42
#endif
#ifndef WAVPLAYER_S3_I2S_LRC
#define WAVPLAYER_S3_I2S_LRC 41
#endif
#ifndef WAVPLAYER_S3_I2S_DIN
#define WAVPLAYER_S3_I2S_DIN 40
#endif
#define WAVPLAYER_I2S_BCLK WAVPLAYER_S3_I2S_BCLK
#define WAVPLAYER_I2S_LRC  WAVPLAYER_S3_I2S_LRC
#define WAVPLAYER_I2S_DIN  WAVPLAYER_S3_I2S_DIN
#define WAVPLAYER_BUTTON   0

#else
#error "Unknown WAVPLAYER_BOARD selection"
#endif

// Start low, especially with a 3-ohm speaker.
#define WAVPLAYER_VOLUME 0.05f
