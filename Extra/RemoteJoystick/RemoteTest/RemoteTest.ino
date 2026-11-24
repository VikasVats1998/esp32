// Remote hardware test  -  ESP32-C3 Super Mini  (streams raw data to the laptop UI)
// Flash this, then run on the laptop:   python3 remote_monitor.py
// Board: ESP32C3 Dev Module, USB CDC On Boot: Enabled.
// Output at 50 Hz:  D,<rawX>,<rawY>,<joyBtn>,<btn>     (ADC 0..4095, buttons 1 = pressed)
// Pins: VRx=GPIO0  VRy=GPIO1  SW=GPIO3  Button=GPIO10 (other leg GND)  VCC=3V3
#define PIN_X    0
#define PIN_Y    1
#define PIN_SW   3
#define PIN_BTN  10

int avg(int pin) { long s = 0; for (int i = 0; i < 8; i++) s += analogRead(pin); return s / 8; }

void setup() {
  Serial.begin(115200);
  pinMode(PIN_SW, INPUT_PULLUP);
  pinMode(PIN_BTN, INPUT_PULLUP);
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
}

void loop() {
  Serial.printf("D,%d,%d,%d,%d\n", avg(PIN_X), avg(PIN_Y),
                digitalRead(PIN_SW) == LOW, digitalRead(PIN_BTN) == LOW);
  delay(20);
}
