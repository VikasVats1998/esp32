// behavior.h - the robot's "personality": turns events into reactions
// (face mood + sound + movement). Runs inside the UI task.
#pragma once
#include "events.h"
#include "config.h"

namespace behavior {
// AUTO    : normal personality (reactions + idle life)
// MANUAL  : no automatic reactions or idle life, nothing sleeps - for testing by hand
// EXPLORE : like AUTO, and Chiku walks around avoiding obstacles
enum Mode : uint8_t { MODE_AUTO, MODE_MANUAL, MODE_EXPLORE };
void setMode(Mode m);
Mode mode();
const char* modeName(Mode m);
bool modeFromName(const char* s, Mode& out);
// UI kit rule "every emotion = face + servo pose + sound": shows the face, plays the matching move
// (motion.cpp pose table) and sound. holdMs: back to the resting face after that (0 = stay).
void express(uint8_t mood, uint32_t holdMs = EMOTION_HOLD_MS, bool sound = true);
void init();
void onEvent(const events::Event& e);
void update(uint32_t dtMs);
}  // namespace behavior
