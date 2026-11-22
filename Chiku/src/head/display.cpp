// display.cpp - see display.h
// CO5300 AMOLED over QSPI with ESP-IDF esp_lcd + DMA. This is the driver of the
// DeskBuddy project (Reference/DeskBuddy/firmware/src/board/display.cpp), which is
// known to work on the Waveshare ESP32-S3-Touch-AMOLED-1.75; only the LVGL colour
// handling differs (we draw RGB565 and swap the bytes while copying to the DMA buffer).
#include "display.h"
#include "config.h"
#include <driver/spi_master.h>
#include <esp_heap_caps.h>
#include <esp_lcd_panel_io.h>
#include <freertos/semphr.h>
#include <draw/sw/lv_draw_sw.h>

namespace display {

namespace {

constexpr int kColOffset = 6;                       // the panel starts at column 6
constexpr int kBufLines = 10;                       // lines per DMA chunk
constexpr size_t kBufBytes = LCD_W * kBufLines * 2;
constexpr size_t kDrawBufBytes = (size_t)LCD_W * LCD_H * 2;
constexpr int kQspiHz = 40000000;                   // DeskBuddy runs 80 MHz; 40 is safer

esp_lcd_panel_io_handle_t io = nullptr;
uint8_t* txBuf[2] = {nullptr, nullptr};
int txCur = 0;
SemaphoreHandle_t txFree = nullptr;
uint8_t curBrightness = 0;
volatile uint32_t flushes = 0;
lv_display_t* dispHandle = nullptr;
uint8_t* rotBuf = nullptr;                          // PSRAM scratch for software rotation

inline uint32_t cmdWord(uint8_t op, uint8_t cmd) { return ((uint32_t)op << 24) | ((uint32_t)cmd << 8); }

void writeCmd(uint8_t cmd, const uint8_t* data = nullptr, size_t len = 0) {
  esp_lcd_panel_io_tx_param(io, cmdWord(0x02, cmd), data, len);
}

void setWindow(int x1, int y1, int x2, int y2) {
  x1 += kColOffset;
  x2 += kColOffset;
  uint8_t ca[4] = {(uint8_t)(x1 >> 8), (uint8_t)x1, (uint8_t)(x2 >> 8), (uint8_t)x2};
  uint8_t ra[4] = {(uint8_t)(y1 >> 8), (uint8_t)y1, (uint8_t)(y2 >> 8), (uint8_t)y2};
  writeCmd(0x2A, ca, 4);
  writeCmd(0x2B, ra, 4);
}

bool IRAM_ATTR onTxDone(esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t*, void*) {
  BaseType_t woken = pdFALSE;
  xSemaphoreGiveFromISR(txFree, &woken);
  return woken == pdTRUE;
}

// LVGL finished an area (RGB565 in PSRAM): swap bytes into a DMA buffer, send in strips.
void flushCb(lv_display_t* disp, const lv_area_t* areaIn, uint8_t* px) {
  const lv_area_t* area = areaIn;
  lv_area_t rotated;
  const lv_display_rotation_t rot = lv_display_get_rotation(disp);
  if (rot != LV_DISPLAY_ROTATION_0 && rotBuf) {      // turn the finished tile into panel orientation
    rotated = *areaIn;
    lv_display_rotate_area(disp, &rotated);
    const int32_t sw = lv_area_get_width(areaIn), sh2 = lv_area_get_height(areaIn);
    lv_draw_sw_rotate(px, rotBuf, sw, sh2, lv_draw_buf_width_to_stride(sw, LV_COLOR_FORMAT_RGB565),
                      lv_draw_buf_width_to_stride(lv_area_get_width(&rotated), LV_COLOR_FORMAT_RGB565), rot,
                      LV_COLOR_FORMAT_RGB565);
    area = &rotated;
    px = rotBuf;
  }
  const int32_t w = lv_area_get_width(area), h = lv_area_get_height(area);
  const uint32_t stride = lv_draw_buf_width_to_stride(w, LV_COLOR_FORMAT_RGB565);   // bytes per row
  for (int32_t y = 0; y < h; y += kBufLines) {
    const int32_t sh = LV_MIN(kBufLines, h - y);
    xSemaphoreTake(txFree, pdMS_TO_TICKS(50));       // never wait forever
    uint16_t* dst = (uint16_t*)txBuf[txCur];
    for (int32_t r = 0; r < sh; r++) {
      const uint16_t* s = (const uint16_t*)(px + (size_t)(y + r) * stride);
      for (int32_t i = 0; i < w; i++) dst[r * w + i] = __builtin_bswap16(s[i]);
    }
    setWindow(area->x1, area->y1 + y, area->x2, area->y1 + y + sh - 1);
    esp_lcd_panel_io_tx_color(io, cmdWord(0x32, 0x2C), txBuf[txCur], (size_t)w * sh * 2);
    txCur ^= 1;
  }
  flushes++;
  lv_display_flush_ready(disp);
}

// The CO5300 only accepts windows with even start and odd end coordinates.
void roundAreaCb(lv_event_t* e) {
  auto* r = (lv_area_t*)lv_event_get_param(e);
  r->x1 &= ~1;
  r->y1 &= ~1;
  r->x2 |= 1;
  r->y2 |= 1;
}

bool initPanel() {
  spi_bus_config_t bus = {};
  bus.data0_io_num = PIN_LCD_SDIO0;
  bus.data1_io_num = PIN_LCD_SDIO1;
  bus.data2_io_num = PIN_LCD_SDIO2;
  bus.data3_io_num = PIN_LCD_SDIO3;
  bus.sclk_io_num = PIN_LCD_SCLK;
  bus.max_transfer_sz = kBufBytes + 16;
  bus.flags = SPICOMMON_BUSFLAG_QUAD;
  if (spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO) != ESP_OK) return false;

  esp_lcd_panel_io_spi_config_t cfg = {};
  cfg.cs_gpio_num = PIN_LCD_CS;
  cfg.dc_gpio_num = -1;
  cfg.spi_mode = 0;
  cfg.pclk_hz = kQspiHz;
  cfg.trans_queue_depth = 4;
  cfg.on_color_trans_done = onTxDone;
  cfg.lcd_cmd_bits = 32;
  cfg.lcd_param_bits = 8;
  cfg.flags.quad_mode = 1;
  if (esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &cfg, &io) != ESP_OK) return false;

  delay(300);                                        // cold start from USB: let the panel supply settle
  for (int pass = 0; pass < 2; pass++) {             // twice: the first one can be lost at power-up
    if (pass) delay(300);
    pinMode(PIN_LCD_RST, OUTPUT);                      // hardware reset
    digitalWrite(PIN_LCD_RST, HIGH);
    delay(10);
    digitalWrite(PIN_LCD_RST, LOW);
    delay(200);
    digitalWrite(PIN_LCD_RST, HIGH);
    delay(200);
    writeCmd(0x11);                                    // sleep out
    delay(120);
    const uint8_t v00 = 0x00, v80 = 0x80, v55 = 0x55, v20 = 0x20, vFF = 0xFF;
    writeCmd(0xFE, &v00, 1);
    writeCmd(0xC4, &v80, 1);                           // SPI mode
    writeCmd(0x3A, &v55, 1);                           // 16 bits per pixel
    writeCmd(0x53, &v20, 1);
    writeCmd(0x63, &vFF, 1);                           // brightness HBM
    writeCmd(0x36, &v00, 1);                           // MADCTL: RGB, no flip
    writeCmd(0x20);                                    // inversion off
    writeCmd(0x29);                                    // display on
    writeCmd(0x58, &v00, 1);                           // high contrast off
  }
  setBrightness(0);
  delay(10);
  return true;
}

// Whole screen one colour (RGB565, native order).
void fill(uint16_t color) {
  uint16_t c = __builtin_bswap16(color);
  uint16_t* d = (uint16_t*)txBuf[0];
  for (size_t i = 0; i < kBufBytes / 2; i++) d[i] = c;
  for (int y = 0; y < LCD_H; y += kBufLines) {
    int y2 = LV_MIN(y + kBufLines, LCD_H) - 1;
    xSemaphoreTake(txFree, pdMS_TO_TICKS(50));
    setWindow(0, y, LCD_W - 1, y2);
    esp_lcd_panel_io_tx_color(io, cmdWord(0x32, 0x2C), txBuf[0], (size_t)LCD_W * (y2 - y + 1) * 2);
  }
  delay(40);
}

}  // namespace

bool init() {
  txBuf[0] = (uint8_t*)heap_caps_malloc(kBufBytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  txBuf[1] = (uint8_t*)heap_caps_malloc(kBufBytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  void* drawBuf = heap_caps_malloc(kDrawBufBytes, MALLOC_CAP_SPIRAM);
  if (!txBuf[0] || !txBuf[1] || !drawBuf) return false;
  txFree = xSemaphoreCreateCounting(2, 2);
  if (!initPanel()) return false;

  fill(0x0000);
  setBrightness(200);
#ifdef DISPLAY_PROBE
  const uint16_t cols[] = {0xF800, 0x07E0, 0x001F, 0xFFFF, 0x0000};   // red green blue white black
  for (uint16_t c : cols) { fill(c); delay(1500); }
#endif

  lv_init();
  lv_tick_set_cb([]() -> uint32_t { return millis(); });
  rotBuf = (uint8_t*)heap_caps_malloc(kDrawBufBytes, MALLOC_CAP_SPIRAM);
  lv_display_t* disp = lv_display_create(LCD_W, LCD_H);
  dispHandle = disp;
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
  lv_display_set_buffers(disp, drawBuf, nullptr, kDrawBufBytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(disp, flushCb);
  lv_display_add_event_cb(disp, roundAreaCb, LV_EVENT_INVALIDATE_AREA, nullptr);
  lv_obj_set_style_bg_color(lv_screen_active(), lv_color_black(), 0);
  return true;
}

void setBrightness(uint8_t v) {
  curBrightness = v;
  if (io) writeCmd(0x51, &v, 1);
}

void setRotation(int deg) {
  if (!dispHandle) return;
  lv_display_rotation_t r = deg == 90 ? LV_DISPLAY_ROTATION_90 : deg == 180 ? LV_DISPLAY_ROTATION_180
                          : deg == 270 ? LV_DISPLAY_ROTATION_270 : LV_DISPLAY_ROTATION_0;
  if (lv_display_get_rotation(dispHandle) == r) return;
  lv_display_set_rotation(dispHandle, r);
  lv_obj_invalidate(lv_screen_active());
}

int rotation() { return dispHandle ? (int)lv_display_get_rotation(dispHandle) * 90 : 0; }

uint8_t brightness() { return curBrightness; }
uint32_t flushCount() { return flushes; }

void panelOff() {
  if (io) writeCmd(0x28);                            // display off
}

}  // namespace display
