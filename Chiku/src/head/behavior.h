// behavior.h - the robot's "personality": turns events into reactions
// (face mood + sound + movement). Runs inside the UI task.
#pragma once
#include "events.h"

namespace behavior {
void init();
void onEvent(const events::Event& e);
void update(uint32_t dtMs);
}  // namespace behavior
