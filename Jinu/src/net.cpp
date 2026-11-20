// net.cpp - Wi-Fi (captive portal on first boot), mDNS jinu.local, web page + JSON API, OTA.
// Runs in its own low priority task.
#include "net.h"
#include "config.h"
#include "logbuf.h"
#include "diag.h"
#include "events.h"
#include "storage.h"
#include "servo.h"
#include "motion.h"
#include "sonar.h"
#include "audio.h"
#include "face.h"
#include "behavior.h"
#include "pads.h"
#include "power.h"
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
<meta name=viewport content="width=device-width,initial-scale=1"><title>Jinu Wi-Fi</title>
<style>body{font-family:sans-serif;background:#111;color:#eee;max-width:420px;margin:20px auto;padding:0 12px}
input,select,button{width:100%;padding:12px;margin:6px 0;font-size:16px;border-radius:8px;border:0;box-sizing:border-box}
button{background:#2aa3ff;color:#fff}</style></head><body><h2>Jinu Wi-Fi setup</h2>
<form method=POST action=/api/wifi><select id=s onchange="n.value=this.value"><option value="">Scanning...</option></select>
<input id=n name=ssid placeholder="Network name"><input name=pass type=password placeholder="Password">
<button>Save and connect</button></form>
<script>fetch('/api/scan').then(r=>r.json()).then(l=>{s.innerHTML='<option value="">Choose network</option>'+l.map(x=>'<option>'+x+'</option>').join('')})</script>
</body></html>)HTML";

static const char NO_FS_PAGE[] PROGMEM =
    "<html><body style='font-family:sans-serif'><h3>Jinu</h3>Web page not uploaded yet. "
    "Run <b>pio run -e head -t uploadfs</b>.<br><a href=/update>Firmware update</a></body></html>";

static const char UPDATE_PAGE[] PROGMEM =
    "<html><body style='font-family:sans-serif'><h3>Jinu firmware update</h3>"
    "<form method=POST action=/update enctype=multipart/form-data>"
    "<input type=file name=fw accept=.bin><input type=submit value=Upload></form></body></html>";

// ---------- helpers ----------
static void sendJson(JsonDocument& d, int code = 200) {
  String out;
  serializeJson(d, out);
  server.send(code, "application/json", out);
}
static void ok(const char* msg = nullptr) {
  JsonDocument d;
  d["ok"] = true;
  if (msg) d["msg"] = msg;
  sendJson(d);
}
static void bad(const char* msg) {
  LOGW("api", "400 %s", msg);
  server.send(400, "text/plain", msg);
}

// Every API call is logged (what the app sent), except the polling calls.
static void logRequest() {
  char args[100] = "";
  for (int i = 0; i < server.args() && strlen(args) < 80; i++) {
    size_t used = strlen(args);
    if (server.argName(i) == "plain") snprintf(args + used, sizeof(args) - used, " <json body>");
    else snprintf(args + used, sizeof(args) - used, " %s=%s", server.argName(i).c_str(), server.arg(i).c_str());
  }
  LOGI("api", "%s %s%s", server.method() == HTTP_GET ? "GET" : "POST", server.uri().c_str(), args);
}
static void route(const char* path, HTTPMethod m, std::function<void()> h, std::function<void()> up = nullptr) {
  bool quiet = !strcmp(path, "/api/state") || !strcmp(path, "/api/events") || !strcmp(path, "/api/log") || !strcmp(path, "/api/diag");
  auto wrapped = [h, quiet]() { if (!quiet) logRequest(); h(); };
  if (up) server.on(path, m, wrapped, up);
  else server.on(path, m, wrapped);
}

static void handleState() {
  Settings& s = storage::get();
  JsonDocument d;
  d["robot"] = "jinu";
  d["api"] = 1;
  d["fw"] = FW_VERSION;
  JsonArray caps = d["caps"].to<JsonArray>();
  for (const char* c : {"display_test", "rgb_led"}) caps.add(c);
  d["name"] = ROBOT_NAME;
  d["uptime"] = millis() / 1000;
  d["rssi"] = WiFi.RSSI();
  d["mode"] = behavior::modeName(behavior::mode());
  d["mute"] = audio::muted();
  d["power"] = (int)power::state();
  d["pads"] = pads::rawState();
  d["padsOnline"] = pads::online();
  d["avoid"] = motion::avoidMode();
  d["gait"] = motion::gaitName(motion::current());
  d["busy"] = motion::busy();
  d["audio"] = audio::online();
  d["ssid"] = s.ssid;
  d["ip"] = ipStr;
  d["brightness"] = s.brightness;
  d["volume"] = s.volume;
  d["heap"] = ESP.getFreeHeap();
  d["dist"] = sonar::distanceCm();
  d["mood"] = face::moodName(face::currentMood());
  JsonArray sv = d["servo"].to<JsonArray>();
  for (int i = 0; i < SERVO_COUNT; i++) {
    JsonObject o = sv.add<JsonObject>();
    o["trim"] = s.servo[i].trim; o["min"] = s.servo[i].minDeg; o["max"] = s.servo[i].maxDeg; o["inv"] = s.servo[i].invert; o["cur"] = (int)servo::current(i);
  }
  sendJson(d);
}

static void handleSet() {
  JsonDocument d;
  if (deserializeJson(d, server.arg("plain"))) { bad("bad json"); return; }
  Settings& s = storage::get();
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
  }
  storage::save();
  events::publish(events::EV_SETTINGS);
  ok();
}

static void handleMotion() {
  motion::Gait g;
  if (server.hasArg("avoid")) { motion::setAvoidMode(server.arg("avoid") == "1"); ok(); return; }
  if (server.hasArg("stop")) { motion::stop(); ok(); return; }
  if (!motion::gaitFromName(server.arg("g").c_str(), g)) { bad("unknown gait"); return; }
  events::publish(events::EV_MOTION_CMD, g, server.hasArg("n") ? server.arg("n").toInt() : 4);
  ok(motion::gaitName(g));
}

static void handleWifiSave() {
  Settings& s = storage::get();
  strlcpy(s.ssid, server.arg("ssid").c_str(), sizeof(s.ssid));
  strlcpy(s.pass, server.arg("pass").c_str(), sizeof(s.pass));
  storage::save();
  server.send(200, "text/html", "<html><body style='font-family:sans-serif'>Saved. Jinu is connecting... "
                                "Join your normal Wi-Fi and open <b>http://jinu.local</b></body></html>");
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
    LOGW("ota", "firmware upload started: %s", up.filename.c_str());
    face::setMood(face::M_LISTENING, 0);
    Update.begin(UPDATE_SIZE_UNKNOWN);
  } else if (up.status == UPLOAD_FILE_WRITE) {
    Update.write(up.buf, up.currentSize);
  } else if (up.status == UPLOAD_FILE_END) {
    LOGI("ota", "upload finished, %u bytes, %s", (unsigned)up.totalSize, Update.end(true) ? "OK" : Update.errorString());
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


// ---------- test / control API (used by the phone app) ----------
static void handleEvents() {
  static events::Record rec[48];
  uint32_t next = 0;
  size_t n = events::history(server.arg("since").toInt(), rec, 48, next);
  JsonDocument d;
  d["next"] = next;
  JsonArray a = d["events"].to<JsonArray>();
  for (size_t i = 0; i < n; i++) {
    JsonObject o = a.add<JsonObject>();
    o["n"] = rec[i].n; o["t"] = rec[i].ms; o["type"] = events::name(rec[i].type); o["a"] = rec[i].a; o["b"] = rec[i].b;
  }
  sendJson(d);
}

static void handleLog() {
  static logbuf::Line rec[60];
  uint32_t next = 0;
  size_t n = logbuf::read(server.arg("since").toInt(), rec, 60, next);
  JsonDocument d;
  d["next"] = next;
  JsonArray a = d["lines"].to<JsonArray>();
  for (size_t i = 0; i < n; i++) {
    JsonObject o = a.add<JsonObject>();
    o["n"] = rec[i].n; o["t"] = rec[i].ms; o["l"] = rec[i].lvl; o["m"] = rec[i].msg;
  }
  sendJson(d);
}


// ---------- hardware diagnostics (GET /api/diag) ----------
static void addLive(JsonArray& a, const char* name, bool ok, const String& detail) {
  JsonObject o = a.add<JsonObject>();
  o["name"] = name; o["ok"] = ok; o["detail"] = detail; o["live"] = true;
}

static void handleDiag() {
  JsonDocument d;
  JsonArray a = d["items"].to<JsonArray>();
  for (size_t i = 0; i < diag::count(); i++) {         // results of the start-up checks
    JsonObject o = a.add<JsonObject>();
    o["name"] = diag::item(i).name; o["ok"] = diag::item(i).ok; o["detail"] = diag::item(i).detail; o["live"] = false;
  }
  addLive(a, "sonar", sonar::distanceCm() > 0, sonar::distanceCm() > 0 ? String(sonar::distanceCm()) + " cm" : "no echo (check ECHO divider)");
  addLive(a, "wifi", WiFi.status() == WL_CONNECTED, String(WiFi.RSSI()) + " dBm");
  d["heap"] = ESP.getFreeHeap();
  d["uptime"] = millis() / 1000;
  sendJson(d);
}

static void handleSim() {                                   // pretend a sensor event happened
  if (server.hasArg("pad")) {
    events::publish(events::EV_PAD, server.arg("pad").toInt(), server.hasArg("g") ? server.arg("g").toInt() : 1);
    ok(); return;
  }
  String ev = server.arg("ev");
  if (ev == "obstacle") events::publish(events::EV_OBSTACLE, 8);
  else if (ev == "button") events::publish(events::EV_BUTTON, 1);
  else { bad("unknown ev"); return; }
  ok();
}

static void handleVoiceList() {
  JsonDocument d;
  JsonArray a = d.to<JsonArray>();
  File dir = LittleFS.open("/voice");
  if (dir && dir.isDirectory()) {
    for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
      String nm = f.name();
      if (nm.endsWith(".wav")) a.add(nm.substring(0, nm.length() - 4));
    }
  }
  sendJson(d);
}

static void setupTestRoutes() {
  route("/api/events", HTTP_GET, handleEvents);
  route("/api/log", HTTP_GET, handleLog);
  route("/api/diag", HTTP_GET, handleDiag);
  route("/api/sim", HTTP_POST, handleSim);
  route("/api/voice/list", HTTP_GET, handleVoiceList);
  route("/api/mode", HTTP_POST, []() {
    behavior::Mode m;
    if (!behavior::modeFromName(server.arg("m").c_str(), m)) { bad("auto|manual|explore"); return; }
    behavior::setMode(m); ok(behavior::modeName(m));
  });
  route("/api/mute", HTTP_POST, []() { audio::setMuted(server.arg("on") == "1"); ok(); });
  route("/api/servo/set", HTTP_POST, []() {
    servo::setTarget(server.arg("ch").toInt(), server.arg("deg").toFloat(), server.hasArg("speed") ? server.arg("speed").toFloat() : 90);
    char m[40]; snprintf(m, sizeof(m), "servo %ld -> %ld", (long)server.arg("ch").toInt(), (long)server.arg("deg").toInt()); ok(m);
  });
  route("/api/servo/release", HTTP_POST, []() { servo::releaseAll(); ok(); });
  route("/api/beep", HTTP_POST, []() {
    audio::beep(constrain((int)server.arg("hz").toInt(), 100, 8000), constrain((int)server.arg("ms").toInt(), 10, 3000)); ok();
  });
  route("/api/melody", HTTP_POST, []() {
    audio::play((audio::Melody)constrain((int)server.arg("id").toInt(), 0, audio::MEL_COUNT - 1), server.arg("loop") == "1"); ok();
  });
  route("/api/audio/stop", HTTP_POST, []() { audio::stop(); ok(); });
  route("/api/look", HTTP_POST, []() { face::lookAt(server.arg("x").toFloat(), server.arg("y").toFloat()); ok(); });
  route("/api/led", HTTP_POST, []() {                       // RGB LED test: r,g,b 0-255, or off=1 to follow the mood again
    if (server.hasArg("off")) face::setLedOverride(-1);
    else face::setLedOverride(((int)server.arg("r").toInt() << 16) | ((int)server.arg("g").toInt() << 8) | (int)server.arg("b").toInt());
    ok();
  });
  route("/api/display", HTTP_POST, []() { face::setTestPattern(server.arg("p").toInt()); ok(); });
  route("/api/reboot", HTTP_POST, []() { ok(); delay(200); ESP.restart(); });
}

static void setupRoutes() {
  setupTestRoutes();
  route("/", HTTP_GET, handleRoot);
  route("/api/state", HTTP_GET, handleState);
  route("/api/set", HTTP_POST, handleSet);
  route("/api/motion", HTTP_POST, handleMotion);
  route("/api/wifi", HTTP_POST, handleWifiSave);
  route("/api/scan", HTTP_GET, handleScan);
  route("/api/wifi/reset", HTTP_POST, []() { storage::clearWifi(); ok(); restartReq = true; });
  route("/api/say", HTTP_POST, []() { audio::say(server.arg("key").c_str(), audio::BAB_HAPPY); ok(audio::hasClip(server.arg("key").c_str()) ? "clip" : "babble"); });
  route("/api/voice", HTTP_POST, []() { server.send(200, "text/plain", "uploaded"); }, handleVoiceUpload);
  route("/api/servo/test", HTTP_POST, []() {
    servo::setTarget(server.arg("ch").toInt(), server.arg("deg").toFloat(), 60); servo::hold(server.arg("ch").toInt()); ok();
  });
  route("/api/mood", HTTP_POST, []() {
    face::setMood((face::Mood)constrain((int)server.arg("m").toInt(), 0, face::MOOD_COUNT - 1), server.hasArg("hold") ? server.arg("hold").toInt() : 5000); ok();
  });
  route("/update", HTTP_GET, []() { server.send_P(200, "text/html", UPDATE_PAGE); });
  route("/update", HTTP_POST, []() {
    server.send(200, "text/plain", Update.hasError() ? "FAILED" : "OK, rebooting");
    delay(300);
    ESP.restart();
  }, handleUpdateUpload);
  server.onNotFound([]() {
    if (portal) { server.sendHeader("Location", "http://192.168.4.1/"); server.send(302, "text/plain", ""); }
    else server.send(404, "text/plain", "not found");
  });
  server.begin();
}

// ---------- connection handling ----------
static void startPortal() {
  portal = true;
  LOGW("wifi", "no Wi-Fi: setup portal \"%s\" (192.168.4.1)", AP_SSID);
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
  LOGI("wifi", "connected to %s, IP %s, RSSI %d", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(), WiFi.RSSI());
  strlcpy(ipStr, WiFi.localIP().toString().c_str(), sizeof(ipStr));
  WiFi.setSleep(true);                                  // modem sleep saves power
  stopPortal();
  MDNS.begin(ROBOT_NAME);
  MDNS.addService("http", "tcp", 80);
  ArduinoOTA.setHostname(ROBOT_NAME);
  ArduinoOTA.begin();
  events::publish(events::EV_WIFI, 1);
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
    if (staUp) ArduinoOTA.handle();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void init() {
  xTaskCreate(netTask, "net", 8192, nullptr, 1, nullptr);
}

}  // namespace net
