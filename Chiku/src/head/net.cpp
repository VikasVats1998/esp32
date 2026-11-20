// net.cpp - see net.h
#include "net.h"
#include "config.h"
#include "events.h"
#include "storage.h"
#include "rtc_time.h"
#include "weather.h"
#include "servo.h"
#include "motion.h"
#include "alarm_timer.h"
#include "camlink.h"
#include "sonar.h"
#include "power.h"
#include "audio.h"
#include "face.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <Update.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

namespace net {

static WebServer server(80);
static DNSServer dns;
static bool portal = false;
static bool staUp = false;
static bool restartReq = false;
static char ipStr[20] = "";

bool connected() { return staUp; }
bool portalActive() { return portal; }
const char* ip() { return ipStr; }
void restartWifi() { restartReq = true; }

// ---------- pages ----------
static const char SETUP_PAGE[] PROGMEM = R"HTML(<!doctype html><html><head><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1"><title>Chiku Wi-Fi</title>
<style>body{font-family:sans-serif;background:#111;color:#eee;max-width:420px;margin:20px auto;padding:0 12px}
input,select,button{width:100%;padding:12px;margin:6px 0;font-size:16px;border-radius:8px;border:0;box-sizing:border-box}
button{background:#2aa3ff;color:#fff}</style></head><body><h2>Chiku Wi-Fi setup</h2>
<form method=POST action=/api/wifi><select id=s onchange="n.value=this.value"><option value="">Scanning...</option></select>
<input id=n name=ssid placeholder="Network name"><input name=pass type=password placeholder="Password">
<button>Save and connect</button></form>
<script>fetch('/api/scan').then(r=>r.json()).then(l=>{s.innerHTML='<option value="">Choose network</option>'+l.map(x=>'<option>'+x+'</option>').join('')})</script>
</body></html>)HTML";

static const char NO_FS_PAGE[] PROGMEM =
    "<html><body style='font-family:sans-serif'><h3>Chiku</h3>Web page not uploaded yet. "
    "Run <b>pio run -e head -t uploadfs</b>.<br><a href=/update>Firmware update</a></body></html>";

static const char UPDATE_PAGE[] PROGMEM =
    "<html><body style='font-family:sans-serif'><h3>Chiku firmware update</h3>"
    "<form method=POST action=/update enctype=multipart/form-data>"
    "<input type=file name=fw accept=.bin><input type=submit value=Upload></form></body></html>";

// ---------- helpers ----------
static void sendJson(JsonDocument& d, int code = 200) {
  String out;
  serializeJson(d, out);
  server.send(code, "application/json", out);
}
static void ok() { server.send(200, "application/json", "{\"ok\":true}"); }

static void handleState() {
  Settings& s = storage::get();
  JsonDocument d;
  d["name"] = ROBOT_NAME;
  d["ssid"] = s.ssid;
  d["ip"] = ipStr;
  d["lat"] = s.lat;
  d["lon"] = s.lon;
  d["brightness"] = s.brightness;
  d["volume"] = s.volume;
  d["heap"] = ESP.getFreeHeap();
  d["psram"] = ESP.getFreePsram();
  d["batt"] = power::batteryPercent();
  d["dist"] = sonar::distanceCm();
  JsonArray sv = d["servo"].to<JsonArray>();
  for (int i = 0; i < SERVO_COUNT; i++) {
    JsonObject o = sv.add<JsonObject>();
    o["trim"] = s.servo[i].trim; o["min"] = s.servo[i].minDeg;
    o["max"] = s.servo[i].maxDeg; o["inv"] = s.servo[i].invert;
  }
  JsonArray al = d["alarms"].to<JsonArray>();
  for (int i = 0; i < MAX_ALARMS; i++) {
    JsonObject o = al.add<JsonObject>();
    o["on"] = s.alarms[i].enabled; o["h"] = s.alarms[i].hour;
    o["m"] = s.alarms[i].minute; o["days"] = s.alarms[i].days;
  }
  JsonObject cam = d["cam"].to<JsonObject>();
  cam["alive"] = camlink::alive(); cam["ip"] = camlink::ip(); cam["state"] = camlink::camState();
  char url[48]; camlink::streamUrl(url, sizeof(url));
  cam["stream"] = url;
  const WeatherData& w = weather::data();
  JsonObject wo = d["weather"].to<JsonObject>();
  wo["valid"] = w.valid; wo["temp"] = w.temp; wo["min"] = w.tmin; wo["max"] = w.tmax;
  wo["text"] = weather::describe(w.code);
  JsonArray tm = d["timers"].to<JsonArray>();
  for (int i = 0; i < alarm_timer::MAX_TIMERS; i++) tm.add(alarm_timer::timerRemainingMs(i) / 1000);
  sendJson(d);
}

static void handleSet() {
  JsonDocument d;
  if (deserializeJson(d, server.arg("plain"))) { server.send(400, "text/plain", "bad json"); return; }
  Settings& s = storage::get();
  if (d["lat"].is<float>()) s.lat = d["lat"];
  if (d["lon"].is<float>()) s.lon = d["lon"];
  if (d["brightness"].is<int>()) s.brightness = constrain((int)d["brightness"], 5, 255);
  if (d["volume"].is<int>()) s.volume = constrain((int)d["volume"], 0, 100);
  if (d["servo"].is<JsonArray>()) {
    int i = 0;
    for (JsonObject o : d["servo"].as<JsonArray>()) {
      if (i >= SERVO_COUNT) break;
      s.servo[i].trim = constrain((int)(o["trim"] | s.servo[i].trim), -45, 45);
      s.servo[i].minDeg = constrain((int)(o["min"] | s.servo[i].minDeg), 0, 180);
      s.servo[i].maxDeg = constrain((int)(o["max"] | s.servo[i].maxDeg), 0, 180);
      s.servo[i].invert = o["inv"] | s.servo[i].invert;
      i++;
    }
    s.servoCalibrated = true;
  }
  if (d["alarms"].is<JsonArray>()) {
    int i = 0;
    for (JsonObject o : d["alarms"].as<JsonArray>()) {
      if (i >= MAX_ALARMS) break;
      s.alarms[i].enabled = o["on"] | false;
      s.alarms[i].hour = constrain((int)(o["h"] | 7), 0, 23);
      s.alarms[i].minute = constrain((int)(o["m"] | 0), 0, 59);
      s.alarms[i].days = (o["days"] | 127) & 0x7F;
      i++;
    }
  }
  storage::save();
  events::publish(events::EV_SETTINGS);
  weather::refreshNow();
  ok();
}

static void handleMotion() {
  motion::Gait g;
  if (server.hasArg("avoid")) { motion::setAvoidMode(server.arg("avoid") == "1"); ok(); return; }
  if (!motion::gaitFromName(server.arg("g").c_str(), g)) { server.send(400, "text/plain", "unknown gait"); return; }
  events::publish(events::EV_MOTION_CMD, g, server.hasArg("n") ? server.arg("n").toInt() : 4);
  ok();
}

static void handleCam() {
  String c = server.arg("cmd");
  if (c == "wake") camlink::wake();
  else if (c == "sleep") camlink::sleepCam();
  else if (c == "led_on") camlink::led(true);
  else if (c == "led_off") camlink::led(false);
  else if (c == "snap") camlink::snap();
  else if (c == "ip") camlink::askIp();
  ok();
}

static void handleWifiSave() {
  Settings& s = storage::get();
  strlcpy(s.ssid, server.arg("ssid").c_str(), sizeof(s.ssid));
  strlcpy(s.pass, server.arg("pass").c_str(), sizeof(s.pass));
  storage::save();
  server.send(200, "text/html", "<html><body style='font-family:sans-serif'>Saved. Chiku is connecting... "
                                "Join your normal Wi-Fi and open <b>http://chiku.local</b></body></html>");
  restartReq = true;
}

static void handleScan() {
  int n = WiFi.scanNetworks();
  JsonDocument d;
  JsonArray a = d.to<JsonArray>();
  for (int i = 0; i < n && i < 15; i++) a.add(WiFi.SSID(i));
  sendJson(d);
}

static void handleRoot() {
  if (portal && !staUp) { server.send_P(200, "text/html", SETUP_PAGE); return; }
  if (storage::fsReady() && LittleFS.exists("/index.html")) {
    File f = LittleFS.open("/index.html", "r");
    server.streamFile(f, "text/html");
    f.close();
  } else {
    server.send_P(200, "text/html", NO_FS_PAGE);
  }
}

static void handleUpdateUpload() {
  HTTPUpload& up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    face::setMood(face::M_LISTENING, 0);
    Update.begin(UPDATE_SIZE_UNKNOWN);
  } else if (up.status == UPLOAD_FILE_WRITE) {
    Update.write(up.buf, up.currentSize);
  } else if (up.status == UPLOAD_FILE_END) {
    Update.end(true);
  }
}

static File voiceFile;
static void handleVoiceUpload() {
  HTTPUpload& up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    String name = up.filename;
    name = name.substring(name.lastIndexOf('/') + 1);            // no folders
    if (!name.endsWith(".wav") || name.length() > 30) return;
    LittleFS.mkdir("/voice");
    voiceFile = LittleFS.open("/voice/" + name, "w");
  } else if (up.status == UPLOAD_FILE_WRITE && voiceFile) {
    voiceFile.write(up.buf, up.currentSize);
  } else if (up.status == UPLOAD_FILE_END && voiceFile) {
    voiceFile.close();
  }
}

static void setupRoutes() {
  server.on("/api/say", HTTP_POST, []() {
    audio::say(server.arg("key").c_str(), audio::BAB_HAPPY); ok();
  });
  server.on("/api/voice", HTTP_POST, []() { server.send(200, "text/plain", "uploaded"); }, handleVoiceUpload);
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/state", HTTP_GET, handleState);
  server.on("/api/set", HTTP_POST, handleSet);
  server.on("/api/motion", HTTP_POST, handleMotion);
  server.on("/api/cam", HTTP_POST, handleCam);
  server.on("/api/wifi", HTTP_POST, handleWifiSave);
  server.on("/api/scan", HTTP_GET, handleScan);
  server.on("/api/wifi/reset", HTTP_POST, []() { storage::clearWifi(); ok(); restartReq = true; });
  server.on("/api/timer", HTTP_POST, []() {
    alarm_timer::startTimer(server.arg("sec").toInt()); ok();
  });
  server.on("/api/timer/cancel", HTTP_POST, []() {
    alarm_timer::cancelTimer(server.arg("slot").toInt()); ok();
  });
  server.on("/api/alarm/test", HTTP_POST, []() { alarm_timer::testRing(); ok(); });
  server.on("/api/weather/refresh", HTTP_POST, []() { weather::refreshNow(); ok(); });
  server.on("/api/servo/test", HTTP_POST, []() {
    servo::setTarget(server.arg("ch").toInt(), server.arg("deg").toFloat(), 60); ok();
  });
  server.on("/api/mood", HTTP_POST, []() {
    int m = server.arg("m").toInt();
    face::setMood((face::Mood)constrain(m, 0, face::MOOD_COUNT - 1), 5000); ok();
  });
  server.on("/update", HTTP_GET, []() { server.send_P(200, "text/html", UPDATE_PAGE); });
  server.on("/update", HTTP_POST, []() {
    server.send(200, "text/plain", Update.hasError() ? "FAILED" : "OK, rebooting");
    delay(300);
    ESP.restart();
  }, handleUpdateUpload);
  server.onNotFound([]() {
    if (portal) {                                       // captive portal: send everything to the setup page
      server.sendHeader("Location", "http://192.168.4.1/");
      server.send(302, "text/plain", "");
    } else server.send(404, "text/plain", "not found");
  });
  server.begin();
}

// ---------- connection handling ----------
static void startPortal() {
  portal = true;
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID);
  dns.start(53, "*", WiFi.softAPIP());
}

static void stopPortal() {
  if (!portal) return;
  dns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  portal = false;
}

static bool connectSta(uint32_t timeoutMs) {
  Settings& s = storage::get();
  if (!s.ssid[0]) return false;
  WiFi.mode(portal ? WIFI_AP_STA : WIFI_STA);
  WiFi.setHostname(ROBOT_NAME);
  WiFi.begin(s.ssid, s.pass);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < timeoutMs) vTaskDelay(pdMS_TO_TICKS(250));
  return WiFi.status() == WL_CONNECTED;
}

static void onConnected() {
  staUp = true;
  strlcpy(ipStr, WiFi.localIP().toString().c_str(), sizeof(ipStr));
  WiFi.setSleep(true);                                  // modem sleep saves power
  stopPortal();
  MDNS.begin(ROBOT_NAME);
  MDNS.addService("http", "tcp", 80);
  ArduinoOTA.setHostname(ROBOT_NAME);
  ArduinoOTA.begin();
  rtc_time::startNtp();
  events::publish(events::EV_WIFI, 1);
  weather::refreshNow();
}

static void netTask(void*) {
  setupRoutes();
  for (;;) {
    if (restartReq) {
      restartReq = false;
      staUp = false; ipStr[0] = 0;
      WiFi.disconnect(true);
      vTaskDelay(pdMS_TO_TICKS(200));
      if (!storage::get().ssid[0]) startPortal();
      else if (connectSta(20000)) onConnected();
      else startPortal();
    }
    // First connection attempt after boot
    static bool first = true;
    if (first) {
      first = false;
      if (!storage::get().ssid[0]) startPortal();
      else if (connectSta(20000)) onConnected();
      else startPortal();
    }
    // Lost the network: try again in the background
    static uint32_t lastRetry = 0;
    if (staUp && WiFi.status() != WL_CONNECTED) {
      staUp = false;
      events::publish(events::EV_WIFI, 0);
    }
    if (!staUp && storage::get().ssid[0] && millis() - lastRetry > 30000) {
      lastRetry = millis();
      WiFi.reconnect();
      uint32_t t0 = millis();
      while (WiFi.status() != WL_CONNECTED && millis() - t0 < 8000) vTaskDelay(pdMS_TO_TICKS(250));
      if (WiFi.status() == WL_CONNECTED) onConnected();
    }
    if (portal) dns.processNextRequest();
    server.handleClient();
    if (staUp) { ArduinoOTA.handle(); weather::update(); }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void init() {
  xTaskCreatePinnedToCore(netTask, "net", 10240, nullptr, 1, nullptr, 0);
}

}  // namespace net
