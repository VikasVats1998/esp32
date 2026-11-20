// net.cpp - see net.h
#include "net.h"
#include "config.h"
#include "link.h"
#include "logbuf.h"
#include "storage.h"
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>

namespace net {

static WebServer server(LINK_PORT);
static DNSServer dns;
static bool inPortal = false, staUp = false, otaOn = false, restartReq = false;
static char ipStr[20] = "";

bool connected() { return staUp; }
bool portal() { return inPortal; }
const char* ip() { return ipStr; }

static const char SETUP_PAGE[] PROGMEM = R"HTML(<!doctype html><html><head><meta charset=utf-8><meta name=viewport content="width=device-width,initial-scale=1">
<title>Desk Wi-Fi</title><style>body{font-family:sans-serif;background:#111;color:#eee;max-width:420px;margin:20px auto;padding:0 12px}
input,button{width:100%;padding:12px;margin:6px 0;font-size:16px;border-radius:8px;border:0;box-sizing:border-box}button{background:#2aa3ff;color:#fff}</style></head>
<body><h2>Desk module Wi-Fi</h2><form method=POST action=/api/wifi><input name=ssid placeholder="Network name"><input name=pass type=password placeholder="Password">
<button>Save and connect</button></form></body></html>)HTML";

static void startPortal() {
  inPortal = true;
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID);
  dns.start(53, "*", WiFi.softAPIP());
  LOGW("wifi", "no Wi-Fi saved: setup portal \"%s\" (192.168.4.1)", AP_SSID);
}

static bool connectSta(uint32_t timeoutMs) {
  Settings& s = storage::get();
  if (!s.ssid[0]) return false;
  WiFi.mode(inPortal ? WIFI_AP_STA : WIFI_STA);
  WiFi.setHostname(DEVICE_NAME);
  WiFi.begin(s.ssid, s.pass);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < timeoutMs) vTaskDelay(pdMS_TO_TICKS(250));
  return WiFi.status() == WL_CONNECTED;
}

static void onConnected() {
  staUp = true;
  strlcpy(ipStr, WiFi.localIP().toString().c_str(), sizeof(ipStr));
  WiFi.setSleep(false);                                   // lowest latency for button presses
  if (inPortal) { dns.stop(); WiFi.softAPdisconnect(true); WiFi.mode(WIFI_STA); inPortal = false; }
  MDNS.begin(DEVICE_NAME);
  MDNS.addService("desk", "tcp", LINK_PORT);
  LOGI("wifi", "connected to %s, IP %s, RSSI %d", WiFi.SSID().c_str(), ipStr, WiFi.RSSI());
}

static void handleRoot() {
  JsonDocument d;
  String page = "<html><body style='font-family:sans-serif'><h3>Desk module</h3>Firmware ";
  page += FW_VERSION;
  page += ". Control it with the Chiku / Jinu robots or the phone app (secure link).<br>Paired clients: ";
  page += (int)desklink_server::clientCount();
  page += desklink_server::pairingOpen() ? "<br><b>Pairing window is open</b> (" + String(desklink_server::pairingLeftMs() / 1000) + " s left)" : "<br>Pairing is closed (power-cycle the desk to open it).";
  page += "</body></html>";
  server.send(200, "text/html", page);
}

// Wi-Fi can only be changed while no Wi-Fi is saved (first setup) or while the pairing window is open and the PIN is right.
static void handleWifi() {
  Settings& s = storage::get();
  bool allowed = !s.ssid[0] || (desklink_server::pairingOpen() && server.arg("pin") == s.pin);
  if (!allowed) { LOGW("wifi", "Wi-Fi change refused"); server.send(403, "text/plain", "not allowed"); return; }
  strlcpy(s.ssid, server.arg("ssid").c_str(), sizeof(s.ssid));
  strlcpy(s.pass, server.arg("pass").c_str(), sizeof(s.pass));
  storage::save();
  server.send(200, "text/html", "<html><body style='font-family:sans-serif'>Saved. The desk module is connecting...</body></html>");
  restartReq = true;
}

static void netTask(void*) {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/wifi", HTTP_POST, handleWifi);
  server.onNotFound([]() {
    if (inPortal) { server.sendHeader("Location", "http://192.168.4.1/"); server.send(302, "text/plain", ""); }
    else server.send(404, "text/plain", "not found");
  });
  desklink_server::init(server);
  server.begin();
  bool first = true;
  uint32_t lastRetry = 0;
  for (;;) {
    if (first || restartReq) {
      first = false;
      restartReq = false;
      staUp = false;
      WiFi.disconnect(true);
      vTaskDelay(pdMS_TO_TICKS(200));
      if (!storage::get().ssid[0]) startPortal();
      else if (connectSta(20000)) onConnected();
      else startPortal();
    }
    if (staUp && WiFi.status() != WL_CONNECTED) { staUp = false; LOGW("wifi", "connection lost"); }
    if (!staUp && !inPortal && millis() - lastRetry > 15000) {
      lastRetry = millis();
      if (connectSta(8000)) onConnected();
    }
    if (inPortal) dns.processNextRequest();
    server.handleClient();
    desklink_server::update();
    // OTA only while the pairing window is open (power-cycle the desk to update), password = the module's PIN
    bool wantOta = staUp && desklink_server::pairingOpen();
    if (wantOta && !otaOn) {
      ArduinoOTA.setHostname(DEVICE_NAME);
      ArduinoOTA.setPassword(storage::get().pin);
      ArduinoOTA.begin();
      otaOn = true;
      LOGI("ota", "OTA enabled during the pairing window, password = PIN %s", storage::get().pin);
    } else if (!wantOta && otaOn) {
      ArduinoOTA.end();
      otaOn = false;
      LOGI("ota", "OTA disabled");
    }
    if (otaOn) ArduinoOTA.handle();
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

void init() { xTaskCreatePinnedToCore(netTask, "net", 10240, nullptr, 1, nullptr, 0); }

}  // namespace net
