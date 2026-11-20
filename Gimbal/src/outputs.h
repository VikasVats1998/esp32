// outputs.h - what the module does to the gimbal's electronics.
//  * Joystick: a PWM pin per axis through an RC filter into the joystick wiper node. While the remote is idle the pin is
//    Hi-Z (input), so the physical joystick works normally.
//  * Buttons: GPIO -> photorelay / optocoupler across the button contacts (HIGH = pressed).
// Every output is "released" at boot, on link loss and on errors.
#pragma once
#include <Arduino.h>

namespace outputs {

void init();                                            // call first in setup(): keys LOW, stick pins Hi-Z
void update();                                          // enforces the maximum key hold time

// sticks: -127..127 per axis (zoom only if ZOOM_ANALOG)
void setStick(int8_t x, int8_t y, int8_t zoom);
void centerSticks();                                    // drive the centre voltage
void releaseSticks();                                   // Hi-Z: the physical joystick is in charge again
bool injecting();

// keys: bit mask of remotelink::KeyBit; remote keys and macro keys are separate sources, both are pressed
void setRemoteKeys(uint16_t mask);
void setMacroKeys(uint16_t mask);
void releaseAllKeys();
uint16_t keysPressed();

// calibration helpers
bool canLearn();                                        // ADC wires connected?
void learnStart();                                      // sticks Hi-Z, records centre and range of the PHYSICAL joystick
bool learning();
bool learnStop();                                       // computes and saves the calibration
int wiperMv(int axis);                                  // mV on the wiper node (-1 = not wired)
const char* keyName(int bit);
bool keyFromName(const char* name, int& bit);
}  // namespace outputs
