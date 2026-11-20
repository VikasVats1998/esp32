// diag.cpp - see diag.h
#include "diag.h"
#include "logbuf.h"

namespace diag {
static const size_t MAX_ITEMS = 16;
static Item items[MAX_ITEMS];
static size_t n = 0;

void set(const char* name, bool ok, const char* detail) {
  size_t i = 0;
  while (i < n && strcmp(items[i].name, name)) i++;       // update an existing entry
  if (i == n) { if (n >= MAX_ITEMS) return; n++; }
  strlcpy(items[i].name, name, sizeof(items[i].name));
  items[i].ok = ok;
  strlcpy(items[i].detail, detail, sizeof(items[i].detail));
  if (ok) LOGI("diag", "%s: ok %s", name, detail);
  else LOGE("diag", "%s: FAILED %s", name, detail);
}
size_t count() { return n; }
const Item& item(size_t i) { return items[i]; }
}  // namespace diag
