// selftest.h - hardware self-test. Hold BOOT while powering on.
// Tests: display, servos (each moves slowly), pads, sonar, speaker, voice clip.
// Report on the screen and on serial (115200). Press BOOT at the end to reboot.
#pragma once
namespace selftest {
bool requested();
void run();       // never returns
}
