// weather.cpp - see weather.h
#include "weather.h"
#include "config.h"
#include "events.h"
#include "storage.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

namespace weather {

static WeatherData W = {};
static uint32_t lastTry = 0;
static bool forceNow = true;

void init() {}
void refreshNow() { forceNow = true; }
const WeatherData& data() { return W; }

const char* describe(int c) {
  if (c == 0) return "Clear";
  if (c <= 2) return "Partly cloudy";
  if (c == 3) return "Cloudy";
  if (c == 45 || c == 48) return "Fog";
  if (c >= 51 && c <= 57) return "Drizzle";
  if (c >= 61 && c <= 67) return "Rain";
  if (c >= 71 && c <= 77) return "Snow";
  if (c >= 80 && c <= 82) return "Showers";
  if (c == 85 || c == 86) return "Snow showers";
  if (c >= 95) return "Thunderstorm";
  return "?";
}

static bool fetch() {
  Settings& s = storage::get();
  char url[256];
  snprintf(url, sizeof(url),
           "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
           "&current=temperature_2m,weather_code,is_day"
           "&daily=temperature_2m_max,temperature_2m_min&timezone=auto&forecast_days=1",
           s.lat, s.lon);
  WiFiClientSecure client;
  client.setInsecure();               // public weather data only, no secrets are sent
  HTTPClient http;
  http.setTimeout(8000);
  if (!http.begin(client, url)) return false;
  int code = http.GET();
  if (code != 200) { http.end(); return false; }
  JsonDocument filter;
  filter["current"]["temperature_2m"] = true;
  filter["current"]["weather_code"] = true;
  filter["current"]["is_day"] = true;
  filter["daily"]["temperature_2m_max"][0] = true;
  filter["daily"]["temperature_2m_min"][0] = true;
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (err) return false;
  W.temp = doc["current"]["temperature_2m"] | 0.f;
  W.code = doc["current"]["weather_code"] | 0;
  W.isDay = (doc["current"]["is_day"] | 1) == 1;
  W.tmax = doc["daily"]["temperature_2m_max"][0] | W.temp;
  W.tmin = doc["daily"]["temperature_2m_min"][0] | W.temp;
  W.valid = true;
  W.updatedMs = millis();
  return true;
}

void update() {
  if (WiFi.status() != WL_CONNECTED) return;
  uint32_t now = millis();
  uint32_t period = W.valid ? WEATHER_PERIOD_MS : 30000UL;     // retry sooner if we have nothing
  if (!forceNow && lastTry && now - lastTry < period) return;
  forceNow = false;
  lastTry = now ? now : 1;
  if (fetch()) events::publish(events::EV_WEATHER);
}

}  // namespace weather
