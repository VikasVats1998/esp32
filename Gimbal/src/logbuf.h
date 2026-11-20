// logbuf.h - one place for all log lines. Every line goes to the serial port AND
// into a ring buffer that the phone app reads with GET /api/log. It also captures
// the ESP-IDF / Arduino system logs (Wi-Fi, I2C errors ...) through esp_log_set_vprintf.
// Use: LOGI("motion", "gait %s x%d", name, cycles);
#pragma once
#include <Arduino.h>

namespace logbuf {

enum Level : uint8_t { L_ERR = 0, L_WARN = 1, L_INFO = 2, L_DEBUG = 3 };

struct Line {
  uint32_t n;       // sequence number (1, 2, 3 ...)
  uint32_t ms;      // uptime in ms
  uint8_t lvl;      // Level
  char msg[112];    // "[tag] text"
};

void init();                                   // call first thing in setup()
void log(Level lvl, const char* tag, const char* fmt, ...) __attribute__((format(printf, 3, 4)));
// Copies lines with n > since (oldest first). 'next' = highest n stored so far.
size_t read(uint32_t since, Line* out, size_t max, uint32_t& next);

}  // namespace logbuf

#define LOGE(tag, ...) logbuf::log(logbuf::L_ERR, tag, __VA_ARGS__)
#define LOGW(tag, ...) logbuf::log(logbuf::L_WARN, tag, __VA_ARGS__)
#define LOGI(tag, ...) logbuf::log(logbuf::L_INFO, tag, __VA_ARGS__)
#define LOGD(tag, ...) logbuf::log(logbuf::L_DEBUG, tag, __VA_ARGS__)
