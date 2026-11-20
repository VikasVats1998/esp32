// behavior.h - Jinu's personality: events -> face mood + sound + body language.
// Runs inside the face task. Edit onPad() in behavior.cpp to change reactions.
#pragma once
#include "events.h"

namespace behavior {
// AUTO    : normal personality (reactions + idle life)
// MANUAL  : no automatic reactions or idle life, nothing sleeps - for testing by hand
// EXPLORE : like AUTO, and Jinu walks around avoiding obstacles
enum Mode : uint8_t { MODE_AUTO, MODE_MANUAL, MODE_EXPLORE };
void setMode(Mode m);
Mode mode();
const char* modeName(Mode m);
bool modeFromName(const char* s, Mode& out);
void init();
void onEvent(const events::Event& e);
void update(uint32_t dtMs);
}  // namespace behavior
