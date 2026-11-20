// diag.h - result of every hardware init at boot, for the app's hardware checklist
// (GET /api/diag). Modules are tested at start-up; main.cpp records the result here.
#pragma once
#include <Arduino.h>

namespace diag {
struct Item {
  char name[14];
  bool ok;
  char detail[32];
};
void set(const char* name, bool ok, const char* detail = "");
size_t count();
const Item& item(size_t i);
}  // namespace diag
