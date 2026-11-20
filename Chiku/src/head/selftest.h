// selftest.h - hardware self-test. Hold BOOT while powering on.
// Tests: display, touch, IMU, RTC, speaker, PCA9685 + servos, sonar, cam link.
// Prints a report on the screen and on the serial port, then waits for a touch.
#pragma once
namespace selftest {
bool requested();     // true if BOOT is held right now (call early in setup)
void run();           // never returns
}
