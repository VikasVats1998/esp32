// weather.h - Open-Meteo (no API key). Fetches every 15 min while Wi-Fi is up.
#pragma once
#include <Arduino.h>

struct WeatherData {
  bool valid;
  float temp, tmin, tmax;   // deg C
  int code;                 // WMO weather code
  bool isDay;
  uint32_t updatedMs;
};

namespace weather {
void init();
void update();                       // net task (blocking HTTP, low priority)
void refreshNow();                   // fetch on next update()
const WeatherData& data();
const char* describe(int code);      // "Rain", "Clear" ...
}  // namespace weather
