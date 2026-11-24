// camlink.cpp - see camlink.h
#include "camlink.h"
#include "config.h"
#include "logbuf.h"
#include "events.h"
#include "storage.h"
#include <ArduinoJson.h>

namespace camlink {

static HardwareSerial& port = Serial1;
static char line[256];
static size_t len = 0;
static uint32_t lastHeard = 0;
static char ipStr[20] = "";
static char state[8] = "?";
static bool wasAlive = false;
static int snapBytes = 0;
static uint32_t lastAsk = 0;

void init() {
  port.setRxBufferSize(512);
  port.begin(CAM_BAUD, SERIAL_8N1, PIN_CAM_RX, PIN_CAM_TX);
}

bool alive() { return millis() - lastHeard < 5000 && lastHeard != 0; }
bool streamReady() { return alive() && ipStr[0]; }
const char* ip() { return ipStr; }
const char* camState() { return state; }
int lastSnapBytes() { return snapBytes; }

void streamUrl(char* out, size_t n) {
  snprintf(out, n, "http://%s/stream", ipStr[0] ? ipStr : "0.0.0.0");
}

void sendCommand(const char* json) {
  port.print(json);
  port.print('\n');
}

void wake() { sendCommand("{\"cmd\":\"wake\"}"); }
void sleepCam() { sendCommand("{\"cmd\":\"sleep\"}"); }
void snap() { sendCommand("{\"cmd\":\"snap\"}"); }
void askIp() { sendCommand("{\"cmd\":\"ip\"}"); }
void led(bool on) { sendCommand(on ? "{\"cmd\":\"led\",\"on\":true}" : "{\"cmd\":\"led\",\"on\":false}"); }

void sendWifi(const char* ssid, const char* pass) {
  JsonDocument d;
  d["cmd"] = "wifi";
  d["ssid"] = ssid;
  d["pass"] = pass;
  serializeJson(d, port);
  port.print('\n');
}

static void handleLine(const char* s) {
  lastHeard = millis();
  // Fast path for the hot message: {"evt":"dist","cm":42}
  int cm;
  if (sscanf(s, "{\"evt\":\"dist\",\"cm\":%d", &cm) == 1) {
    events::publish(events::EV_DIST_RAW, cm);
    return;
  }
  JsonDocument d;
  if (deserializeJson(d, s)) return;                    // not JSON (boot noise)
  const char* evt = d["evt"] | "";
  if (!strcmp(evt, "ready")) {
    askIp();
    Settings& st = storage::get();
    if (st.ssid[0]) sendWifi(st.ssid, st.pass);         // share Wi-Fi with the cam
  } else if (!strcmp(evt, "ip")) {
    strlcpy(ipStr, d["ip"] | "", sizeof(ipStr));
    LOGI("cam", "ip %s", ipStr);
  } else if (!strcmp(evt, "snap")) {
    snapBytes = d["len"] | 0;
  } else if (!strcmp(evt, "cam")) {
    strlcpy(state, d["state"] | "?", sizeof(state));
  }
}

void update() {
  while (port.available()) {
    char c = port.read();
    if (c == '\n' || c == '\r') {
      if (len) { line[len] = 0; if (line[0] == '{') handleLine(line); len = 0; }
    } else if (len < sizeof(line) - 1) {
      line[len++] = c;
    } else {
      len = 0;                                           // overflow: drop line
    }
  }
  bool a = alive();
  if (a != wasAlive) {
    wasAlive = a;
    LOGI("cam", "link %s", a ? "alive" : "lost");
    events::publish(events::EV_CAM_STATE, a ? 1 : 0);
  }
  // Ask for the IP every 10 s until we know it
  if (a && !ipStr[0] && millis() - lastAsk > 10000) { lastAsk = millis(); askIp(); }
}

}  // namespace camlink
