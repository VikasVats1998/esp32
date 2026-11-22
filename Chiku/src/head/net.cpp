// net.cpp - see net.h
#include "net.h"
#include "config.h"
#include "logbuf.h"
#include "desk.h"
#include "diag.h"
#include "events.h"
#include "storage.h"
#include "rtc_time.h"
#include "weather.h"
#include "srvlink.h"
#include "mic.h"
#include "servo.h"
#include "motion.h"
#include "alarm_timer.h"
#include "camlink.h"
#include "sonar.h"
#include "power.h"
#include "audio.h"
#include "face.h"
#include "behavior.h"
#include "imu.h"
#include "pads.h"
#include "ui.h"
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


// ---------- standing desk accessory ----------
static void deskJson(JsonObject o) {
  desk::State s = desk::state();
  o["paired"] = s.paired;
  o["host"] = desk::host();
  o["ok"] = s.ok;
  o["hv"] = s.hv;
  o["h"] = s.h;
  o["moving"] = s.moving;
  o["dir"] = s.dir;
  o["armed"] = s.armed;
  o["travel"] = s.travel;
  o["src"] = s.src;
  o["err"] = s.err;
  o["age"] = s.ageMs == 0xFFFFFFFF ? -1 : (int)s.ageMs;
}

static void setupDeskRoutes() {
  route("/api/desk", HTTP_GET, []() { JsonDocument d; deskJson(d.to<JsonObject>()); sendJson(d); });
  route("/api/desk/pair", HTTP_POST, []() {
    // The token protects the desk endpoints. It can only be set while none exists (or after unpairing).
    String tok = server.arg("token");
    if (tok.length() < 8) { bad("token (8+ characters) required"); return; }
    if (desk::tokenSet() && !desk::checkToken(tok.c_str())) { LOGW("desk", "pair refused: wrong token"); server.send(401, "text/plain", "wrong token"); return; }
    String host = server.hasArg("host") && server.arg("host").length() ? server.arg("host") : String("desk.local");
    String err;
    if (desk::pair(host.c_str(), err)) { desk::setToken(tok.c_str()); ok("paired"); } else bad(err.c_str());
  });
  route("/api/desk/unpair", HTTP_POST, []() {
    if (!desk::checkToken(server.arg("token").c_str())) { LOGW("desk", "unpair refused: wrong token"); server.send(401, "text/plain", "wrong token"); return; }
    desk::unpair();
    ok("unpaired");
  });
  route("/api/desk/cmd", HTTP_POST, []() {
    String op = server.arg("op");
    if (!desk::checkToken(server.arg("token").c_str())) { LOGW("desk", "desk command refused: missing or wrong token"); server.send(401, "text/plain", "wrong token"); return; }
    if (!desk::state().paired) { bad("desk not paired"); return; }
    if (op == "move") {
      int dir = server.arg("dir") == "up" ? 1 : server.arg("dir") == "down" ? -1 : 0;
      if (!dir) { bad("dir must be up or down"); return; }
      desk::moveHold(dir);
    } else if (op == "stop") { desk::moveHold(0); desk::command("stop"); }
    else if (op == "preset" || op == "save") desk::command(op.c_str(), server.arg("n").toInt());
    else if (op == "tap") desk::command("tap", 0, server.arg("key").c_str(), 0, server.arg("ms").toInt());
    else if (op == "setheight") desk::command("setheight", 0, nullptr, server.arg("h").toFloat());
    else if (op == "arm") desk::command("arm", server.arg("on") == "1");
    else { bad("unknown op"); return; }
    ok(op.c_str());
  });
}

static void handleState() {
  Settings& s = storage::get();
  JsonDocument d;
  d["robot"] = "chiku";
  d["api"] = 1;
  d["fw"] = FW_VERSION;
  JsonArray caps = d["caps"].to<JsonArray>();
  for (const char* c : {"screens", "alarms", "timers", "weather", "imu", "cam", "pan", "battery", "display_test", "desk"}) caps.add(c);
  deskJson(d["desk"].to<JsonObject>());
  d["name"] = ROBOT_NAME;
  d["uptime"] = millis() / 1000;
  d["rssi"] = WiFi.RSSI();
  d["mode"] = behavior::modeName(behavior::mode());
  d["mute"] = audio::muted();
  d["mood"] = face::moodName(face::currentMood());
  d["screen"] = (int)ui::current();
  d["power"] = (int)power::state();
  d["pads"] = pads::rawState();
  d["padsOnline"] = pads::expanderOnline();
  d["avoid"] = motion::avoidMode();
  d["gait"] = motion::gaitName(motion::current());
  d["busy"] = motion::busy();
  JsonObject im = d["imu"].to<JsonObject>();
  float ax, ay, az; imu::getAccel(ax, ay, az);
  im["ok"] = imu::online(); im["x"] = ax; im["y"] = ay; im["z"] = az; im["tilt"] = imu::tiltDeg();
  float gx, gy, gz; imu::getGyro(gx, gy, gz);
  im["gx"] = gx; im["gy"] = gy; im["gz"] = gz;
  JsonObject sv0 = d["hub"].to<JsonObject>();
  sv0["url"] = s.serverUrl; sv0["linked"] = srvlink::linked(); sv0["devices"] = srvlink::count();
  sv0["ai"] = s.aiVoice;
  JsonObject sn = d["sensors"].to<JsonObject>();
  sn["roll"] = imu::roll(); sn["pitch"] = imu::pitch(); sn["flipped"] = imu::flipped();
  sn["gyroCal"] = imu::calibrated();
  sn["gbx"] = imu::gyroBias(0); sn["gby"] = imu::gyroBias(1); sn["gbz"] = imu::gyroBias(2);
  sn["micOk"] = mic::online(); sn["sound"] = mic::level(); sn["soundPeak"] = mic::peak(); sn["noise"] = mic::noiseFloor();
  sn["clapWake"] = s.clapWake; sn["clapSens"] = s.clapSens; sn["wakeWord"] = s.wakeWord;
  sn["batV"] = power::batteryVolts(); sn["usbV"] = power::vbusVolts(); sn["coreC"] = power::coreTempC();
  sn["pmu"] = power::pmuOnline();
  d["ssid"] = s.ssid;
  d["ip"] = ipStr;
  d["lat"] = s.lat;
  d["lon"] = s.lon;
  d["brightness"] = s.brightness;
  d["volume"] = s.volume;
  d["heap"] = ESP.getFreeHeap();
  d["psram"] = ESP.getFreePsram();
  d["batt"] = power::batteryPercent();
  d["charging"] = power::charging();
  d["pca"] = servo::online();
  d["audio"] = audio::online();
  d["dist"] = sonar::distanceCm();
  JsonArray sv = d["servo"].to<JsonArray>();
  for (int i = 0; i < SERVO_COUNT; i++) {
    JsonObject o = sv.add<JsonObject>();
    o["trim"] = s.servo[i].trim; o["min"] = s.servo[i].minDeg;
    o["max"] = s.servo[i].maxDeg; o["inv"] = s.servo[i].invert; o["cur"] = (int)servo::current(i);
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
  cam["snap"] = camlink::lastSnapBytes();
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
  if (deserializeJson(d, server.arg("plain"))) { bad("bad json"); return; }
  Settings& s = storage::get();
  if (d["lat"].is<float>()) s.lat = d["lat"];
  if (d["lon"].is<float>()) s.lon = d["lon"];
  if (d["brightness"].is<int>()) s.brightness = constrain((int)d["brightness"], 5, 255);
  if (d["volume"].is<int>()) s.volume = constrain((int)d["volume"], 0, 100);
  if (d["serverUrl"].is<const char*>()) {                 // "" turns the hub link off
    strlcpy(s.serverUrl, d["serverUrl"], sizeof(s.serverUrl));
    size_t L = strlen(s.serverUrl);
    while (L && s.serverUrl[L - 1] == '/') s.serverUrl[--L] = 0;
    srvlink::syncNow();
  }
  if (d["serverToken"].is<const char*>()) strlcpy(s.serverToken, d["serverToken"], sizeof(s.serverToken));
  if (d["aiVoice"].is<bool>()) s.aiVoice = d["aiVoice"];
  if (d["clapWake"].is<bool>()) s.clapWake = d["clapWake"];
  if (d["clapSens"].is<int>()) s.clapSens = constrain((int)d["clapSens"], 0, 100);
  if (d["wakeWord"].is<bool>()) s.wakeWord = d["wakeWord"];
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
  if (server.hasArg("stop")) { motion::stop(); ok(); return; }
  if (!motion::gaitFromName(server.arg("g").c_str(), g)) { bad("unknown gait"); return; }
  events::publish(events::EV_MOTION_CMD, g, server.hasArg("n") ? server.arg("n").toInt() : 4);
  ok(motion::gaitName(g));
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
static const char* evName(uint8_t t) { return events::name((events::Type)t); }

static void handleEvents() {
  static events::Record rec[48];
  uint32_t next = 0;
  size_t n = events::history(server.arg("since").toInt(), rec, 48, next);
  JsonDocument d;
  d["next"] = next;
  JsonArray a = d["events"].to<JsonArray>();
  for (size_t i = 0; i < n; i++) {
    JsonObject o = a.add<JsonObject>();
    o["n"] = rec[i].n; o["t"] = rec[i].ms; o["type"] = evName(rec[i].type); o["a"] = rec[i].a; o["b"] = rec[i].b;
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
  addLive(a, "sonar", sonar::distanceCm() > 0, sonar::distanceCm() > 0 ? String(sonar::distanceCm()) + " cm" : "no echo (cam link + sensor)");
  float ax, ay, az; imu::getAccel(ax, ay, az);
  float mag = sqrtf(ax * ax + ay * ay + az * az);
  addLive(a, "imu_gravity", imu::online() && mag > 0.8f && mag < 1.2f, String(mag, 2) + " g (expect 1.0)");
  addLive(a, "rtc_time", rtc_time::valid(), rtc_time::valid() ? "time valid" : "no time yet (needs Wi-Fi/NTP)");
  addLive(a, "cam_link", camlink::alive(), camlink::alive() ? String(camlink::camState()) + " " + camlink::ip() : "no reply from ESP32-CAM");
  addLive(a, "battery", power::batteryPercent() >= 0, power::batteryPercent() >= 0 ? String(power::batteryPercent()) + "% " + String(power::batteryVolts(), 2) + " V" : "no battery");
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
  if (ev == "shake") events::publish(events::EV_SHAKE);
  else if (ev == "pickup") events::publish(events::EV_PICKUP);
  else if (ev == "putdown") events::publish(events::EV_PUTDOWN);
  else if (ev == "fall") events::publish(events::EV_FALL);
  else if (ev == "tap") events::publish(events::EV_TAP_IMU);
  else if (ev == "obstacle") events::publish(events::EV_OBSTACLE, 8);
  else if (ev == "touch") events::publish(events::EV_TOUCH_TAP, 233, 233);
  else if (ev == "alarm") alarm_timer::testRing();
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
  route("/api/screen", HTTP_POST, []() { events::publish(events::EV_SCREEN, server.arg("id").toInt()); ok(); });
  route("/api/display", HTTP_POST, []() { ui::setTestPattern(server.arg("p").toInt()); ok(); });
  route("/api/imu/calibrate", HTTP_POST, []() { imu::calibrate(); ok(); });
  route("/api/reboot", HTTP_POST, []() { ok(); delay(200); ESP.restart(); });
}

static void setupRoutes() {
  setupTestRoutes();
  setupDeskRoutes();
  route("/api/say", HTTP_POST, []() {
    audio::say(server.arg("key").c_str(), audio::BAB_HAPPY); ok(audio::hasClip(server.arg("key").c_str()) ? "clip" : "babble");
  });
  route("/api/voice", HTTP_POST, []() { server.send(200, "text/plain", "uploaded"); }, handleVoiceUpload);
  route("/", HTTP_GET, handleRoot);
  route("/api/state", HTTP_GET, handleState);
  route("/api/set", HTTP_POST, handleSet);
  route("/api/motion", HTTP_POST, handleMotion);
  route("/api/cam", HTTP_POST, handleCam);
  route("/api/wifi", HTTP_POST, handleWifiSave);
  route("/api/scan", HTTP_GET, handleScan);
  route("/api/wifi/reset", HTTP_POST, []() { storage::clearWifi(); ok(); restartReq = true; });
  route("/api/timer", HTTP_POST, []() {
    alarm_timer::startTimer(server.arg("sec").toInt()); ok();
  });
  route("/api/timer/cancel", HTTP_POST, []() {
    alarm_timer::cancelTimer(server.arg("slot").toInt()); ok();
  });
  route("/api/alarm/test", HTTP_POST, []() { alarm_timer::testRing(); ok(); });
  route("/api/weather/refresh", HTTP_POST, []() { weather::refreshNow(); ok(); });
  route("/api/servo/test", HTTP_POST, []() {
    servo::setTarget(server.arg("ch").toInt(), server.arg("deg").toFloat(), 60); ok();
  });
  route("/api/mood", HTTP_POST, []() {
    int m = server.arg("m").toInt();
    face::setMood((face::Mood)constrain(m, 0, face::MOOD_COUNT - 1), server.hasArg("hold") ? server.arg("hold").toInt() : 5000); ok();
  });
  route("/update", HTTP_GET, []() { server.send_P(200, "text/html", UPDATE_PAGE); });
  route("/update", HTTP_POST, []() {
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
  rtc_time::startNtp();
  events::publish(events::EV_WIFI, 1);
  weather::refreshNow();
}

static void netTask(void*) {
  WiFi.mode(WIFI_STA);   // brings up the TCP/IP stack; server.begin() crashes (null mutex) without it
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
    if (staUp) { ArduinoOTA.handle(); weather::update(); desk::update(); }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void init() {
  xTaskCreatePinnedToCore(netTask, "net", 10240, nullptr, 1, nullptr, 0);
}

}  // namespace net
