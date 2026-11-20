// behavior.h - Jinu's personality: events -> face mood + sound + body language.
// Runs inside the face task. Edit onPad() in behavior.cpp to change reactions.
#pragma once
#include "events.h"

namespace behavior {
void init();
void onEvent(const events::Event& e);
void update(uint32_t dtMs);
}  // namespace behavior
