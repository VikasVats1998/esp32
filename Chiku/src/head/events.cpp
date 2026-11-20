// events.cpp - see events.h
#include "events.h"

namespace events {

static const uint8_t MAX_SUBS = 12;
static QueueHandle_t subs[MAX_SUBS];
static uint8_t subCount = 0;
static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

void init() { subCount = 0; }

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
  for (uint8_t i = 0; i < subCount; i++) {
    if (xQueueSend(subs[i], &e, 0) != pdTRUE) ok = false;
  }
  return ok;
}

}  // namespace events
