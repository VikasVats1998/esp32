// srvlink.cpp - see srvlink.h
#include "srvlink.h"
#include "config.h"
#include "logbuf.h"
#include "storage.h"
#include "imu.h"
#include "power.h"
#include "mic.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <math.h>

namespace srvlink {

static Device D[MAX_DEVICES];
static bool dirty[MAX_DEVICES];           // changed here, not yet sent
static int n = 0;
static float envT = NAN;
static int envH = -1;
static bool ai = false;
static uint32_t lastOk = 0;
static volatile bool nowFlag = false;
static SemaphoreHandle_t mtx = nullptr;

struct Lock {
  Lock() { xSemaphoreTake(mtx, portMAX_DELAY); }
  ~Lock() { xSemaphoreGive(mtx); }
};

bool enabled() { return storage::get().serverUrl[0] != 0; }
bool linked() { return lastOk && millis() - lastOk < 5000; }
uint32_t ageMs() { return lastOk ? millis() - lastOk : 0xFFFFFFFFu; }
int count() { return n; }
float envTemp() { return envT; }
int envHumidity() { return envH; }
bool aiAvailable() { return ai; }
void syncNow() { nowFlag = true; }

bool get(int i, Device& out) {
  Lock l;
  if (i < 0 || i >= n) return false;
  out = D[i];
  return true;
}

int find(Type t, int nth) {
  Lock l;
  for (int i = 0; i < n; i++)
    if (D[i].type == t && nth-- == 0) return i;
  return -1;
}

void setOn(int i, bool on) {
  { Lock l; if (i < 0 || i >= n) return; D[i].on = on; dirty[i] = true; }
  syncNow();
}

void setLevel(int i, int level) {
  { Lock l;
    if (i < 0 || i >= n) return;
    D[i].level = constrain(level, 0, D[i].type == T_CLIMATE ? 30 : 100);
    if (D[i].type != T_CLIMATE) D[i].on = D[i].level > 0;
    dirty[i] = true; }
  syncNow();
}

void toggle(int i) {
  { Lock l; if (i < 0 || i >= n) return; D[i].on = !D[i].on; dirty[i] = true; }
  syncNow();
}

static Type typeFrom(const char* s) {
  if (!strcmp(s, "light")) return T_LIGHT;
  if (!strcmp(s, "fan")) return T_FAN;
  if (!strcmp(s, "switch")) return T_SWITCH;
  if (!strcmp(s, "climate")) return T_CLIMATE;
  return T_OTHER;
}

// Merge the server's list. Devices we changed since the last send keep our values.
static void apply(JsonDocument& doc) {
  Lock l;
  JsonArray arr = doc["devices"].as<JsonArray>();
  int i = 0;
  for (JsonObject o : arr) {
    if (i >= MAX_DEVICES) break;
    if (strcmp(D[i].id, o["id"] | "") != 0) { dirty[i] = false; }   // list changed on the server
    if (!dirty[i]) {
      strlcpy(D[i].id, o["id"] | "?", sizeof(D[i].id));
      strlcpy(D[i].name, o["name"] | "?", sizeof(D[i].name));
      D[i].type = typeFrom(o["type"] | "");
      D[i].on = o["on"] | false;
      D[i].level = o["level"] | 0;
      D[i].temp = o["temp"] | 0;
      D[i].load = o["load"] | 0.f;
    }
    i++;
  }
  n = i;
  JsonObjectConst cfg = doc["cfg"].as<JsonObjectConst>();      // settings pushed from the dashboard / app
  if (!cfg.isNull()) {
    Settings& st = storage::get();
    bool changed = false;
    if (cfg["clapWake"].is<bool>() && st.clapWake != (bool)cfg["clapWake"]) { st.clapWake = cfg["clapWake"]; changed = true; }
    if (cfg["wakeWord"].is<bool>() && st.wakeWord != (bool)cfg["wakeWord"]) { st.wakeWord = cfg["wakeWord"]; changed = true; }
    if (cfg["clapSens"].is<int>() && st.clapSens != (int)cfg["clapSens"]) { st.clapSens = constrain((int)cfg["clapSens"], 0, 100); changed = true; }
    if (changed) storage::save();
    if (cfg["calibrate"] | false) imu::calibrate();
  }
  envT = doc["env"]["temp_c"].is<float>() ? (float)doc["env"]["temp_c"] : NAN;
  envH = doc["env"]["humidity"] | -1;
  ai = doc["ai"]["enabled"] | false;
}

static bool sync() {
  Settings& s = storage::get();
  JsonDocument req;
  req["id"] = ROBOT_NAME;
  JsonObject t = req["tel"].to<JsonObject>();
  t["fw"] = FW_VERSION;
  t["tilt"] = (int)imu::tiltDeg();
  t["bat"] = power::batteryPercent();
  t["rssi"] = WiFi.RSSI();
  t["heap"] = ESP.getFreeHeap() / 1024;
  t["roll"] = (int)imu::roll();
  t["pitch"] = (int)imu::pitch();
  float gx, gy, gz; imu::getGyro(gx, gy, gz);
  t["gyro"] = (int)sqrtf(gx * gx + gy * gy + gz * gz);          // total rotation speed, deg/s
  t["flip"] = imu::flipped();
  t["batv"] = serialized(String(power::batteryVolts(), 2));
  t["chg"] = power::charging();
  t["usb"] = power::vbusVolts() > 3.5f;
  t["temp"] = (int)power::coreTempC();
  t["snd"] = mic::level();
  t["clapWake"] = s.clapWake;
  t["clapSens"] = s.clapSens;
  t["wakeWord"] = s.wakeWord;
  JsonArray cmds = req["cmds"].to<JsonArray>();
  {
    Lock l;
    for (int i = 0; i < n; i++) {
      if (!dirty[i]) continue;
      JsonObject c = cmds.add<JsonObject>();
      c["dev"] = D[i].id;
      c["on"] = D[i].on;
      c["level"] = D[i].level;
      dirty[i] = false;
    }
  }
  String body;
  serializeJson(req, body);

  char url[112];
  snprintf(url, sizeof(url), "%s/api/sync", s.serverUrl);
  WiFiClient client;
  HTTPClient http;
  http.setTimeout(2500);
  http.setConnectTimeout(2000);
  if (!http.begin(client, url)) return false;
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Token", s.serverToken);
  int code = http.POST(body);
  if (code != 200) { http.end(); return false; }
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, http.getStream());
  http.end();
  if (err) return false;
  apply(doc);
  lastOk = millis();
  return true;
}

// The hub announces itself by UDP broadcast (Server/server.py). We take its address when we have
// none, or when the saved one has not answered for 30 s (the PC got a new IP address).
static WiFiUDP beaconUdp;
static bool beaconOpen = false;
static void listenForHub() {
  if (WiFi.status() != WL_CONNECTED) { beaconOpen = false; return; }
  if (!beaconOpen) beaconOpen = beaconUdp.begin(41234);
  int n = beaconUdp.parsePacket();
  if (n <= 0) return;
  char buf[96];
  int len = beaconUdp.read(buf, sizeof(buf) - 1);
  if (len <= 0) return;
  buf[len] = 0;
  JsonDocument d;
  if (deserializeJson(d, buf) || strcmp(d["hub"] | "", "chiku") != 0) return;
  Settings& s = storage::get();
  bool lost = enabled() && (lastOk == 0 ? millis() > 30000 : millis() - lastOk > 30000);
  if (enabled() && !lost) return;
  char url[64];
  snprintf(url, sizeof(url), "http://%s:%d", beaconUdp.remoteIP().toString().c_str(), (int)(d["port"] | 8080));
  if (strcmp(url, s.serverUrl) == 0) return;
  strlcpy(s.serverUrl, url, sizeof(s.serverUrl));
  storage::save();
  LOGI("srv", "found the hub: %s", url);
  nowFlag = true;
}

static void task(void*) {
  bool wasLinked = false;
  uint32_t last = 0;
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(100));
    listenForHub();
    if (!enabled() || WiFi.status() != WL_CONNECTED) continue;
    uint32_t now = millis();
    if (!nowFlag && now - last < 1000) continue;
    nowFlag = false;
    last = now;
    bool ok = sync();
    if (!ok) last = now + 2000;                 // back off a little when the server is away
    if (ok != wasLinked) {
      wasLinked = ok;
      LOGI("srv", "%s", ok ? "hub server connected" : "hub server lost");
    }
  }
}

void init() {
  mtx = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(task, "srvlink", 8192, nullptr, 1, nullptr, 0);
}

}  // namespace srvlink
