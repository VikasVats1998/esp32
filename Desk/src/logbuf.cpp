// logbuf.cpp - see logbuf.h
#include "logbuf.h"
#include <esp_log.h>
#include <stdarg.h>

namespace logbuf {

static const size_t SIZE = 100;
static Line ring[SIZE];
static uint32_t count = 0;                      // total lines ever written
static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
static vprintf_like_t previous = nullptr;       // the original ESP log output (serial)

static void store(Level lvl, const char* text) {
  portENTER_CRITICAL(&mux);
  Line& l = ring[count % SIZE];
  l.n = count + 1;
  l.ms = millis();
  l.lvl = lvl;
  strlcpy(l.msg, text, sizeof(l.msg));
  count++;
  portEXIT_CRITICAL(&mux);
}

void log(Level lvl, const char* tag, const char* fmt, ...) {
  char buf[sizeof(Line::msg)];
  int n = snprintf(buf, sizeof(buf), "[%s] ", tag);
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf + n, sizeof(buf) - n, fmt, ap);
  va_end(ap);
  store(lvl, buf);
  static const char C[] = {'E', 'W', 'I', 'D'};
  Serial.printf("%10lu %c %s\n", (unsigned long)millis(), C[lvl], buf);
}

// System log lines ("E (1234) wifi: text", with colour codes) -> ring + serial.
static int hook(const char* fmt, va_list ap) {
  char buf[160];
  va_list copy;
  va_copy(copy, ap);
  vsnprintf(buf, sizeof(buf), fmt, copy);
  va_end(copy);
  const char* p = buf;
  while (*p == '\033') { while (*p && *p != 'm') p++; if (*p) p++; }      // skip colour escape
  Level lvl = *p == 'E' ? L_ERR : *p == 'W' ? L_WARN : *p == 'D' || *p == 'V' ? L_DEBUG : L_INFO;
  char clean[sizeof(Line::msg)];
  size_t j = 0;
  for (; *p && j < sizeof(clean) - 1; p++) {
    if (*p == '\033') { while (*p && *p != 'm') p++; continue; }
    if (*p == '\n' || *p == '\r') continue;
    clean[j++] = *p;
  }
  clean[j] = 0;
  if (j) store(lvl, clean);
  return previous ? previous(fmt, ap) : 0;
}

void init() {
  previous = esp_log_set_vprintf(hook);
}

size_t read(uint32_t since, Line* out, size_t max, uint32_t& next) {
  size_t n = 0;
  portENTER_CRITICAL(&mux);
  uint32_t first = count > SIZE ? count - SIZE : 0;
  for (uint32_t i = first; i < count && n < max; i++) {
    const Line& l = ring[i % SIZE];
    if (l.n > since) out[n++] = l;
  }
  next = count;
  portEXIT_CRITICAL(&mux);
  return n;
}

}  // namespace logbuf
