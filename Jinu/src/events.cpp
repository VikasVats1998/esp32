// events.cpp - see events.h
#include "events.h"
#include "logbuf.h"

namespace events {

static const uint8_t MAX_SUBS = 12;
static QueueHandle_t subs[MAX_SUBS];
static uint8_t subCount = 0;
static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

static const size_t HIST = 48;
static Record hist[HIST];
static uint32_t histCount = 0;

static const char* NAMES[EV_TYPE_COUNT] = {"none", "pad", "button", "dist", "obstacle", "wifi", "settings", "motion", "power", "wake"};
const char* name(Type t) { return t < EV_TYPE_COUNT ? NAMES[t] : "?"; }

void init() { subCount = 0; }

size_t history(uint32_t since, Record* out, size_t max, uint32_t& next) {
  size_t n = 0;
  portENTER_CRITICAL(&mux);
  uint32_t first = histCount > HIST ? histCount - HIST : 0;
  for (uint32_t i = first; i < histCount && n < max; i++) {
    const Record& r = hist[i % HIST];
    if (r.n > since) out[n++] = r;
  }
  next = histCount;
  portEXIT_CRITICAL(&mux);
  return n;
}

QueueHandle_t subscribe(uint8_t depth) {
  QueueHandle_t q = xQueueCreate(depth, sizeof(Event));
  portENTER_CRITICAL(&mux);
  if (subCount < MAX_SUBS) subs[subCount++] = q;
  portEXIT_CRITICAL(&mux);
  return q;
}

bool publish(Type type, int32_t a, int32_t b) {
  Event e = {type, a, b};
  bool ok = true;
  if (type != EV_DIST) {
    portENTER_CRITICAL(&mux);
    hist[histCount % HIST] = {histCount + 1, millis(), type, a, b};
    histCount++;
    portEXIT_CRITICAL(&mux);
    logbuf::log(logbuf::L_INFO, "event", "%s a=%ld b=%ld", name(type), (long)a, (long)b);
  }
  for (uint8_t i = 0; i < subCount; i++) {
    if (xQueueSend(subs[i], &e, 0) != pdTRUE) ok = false;
  }
  return ok;
}

}  // namespace events
