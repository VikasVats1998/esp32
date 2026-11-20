// desk.cpp - see desk.h
#include "desk.h"
#include "config.h"
#include "events.h"
#include "logbuf.h"
#include "deskclient.h"
#include <ArduinoJson.h>
#include <Preferences.h>
#include <WiFi.h>

namespace desk {

static DeskClient client;
static State st = {};
static uint32_t lastOk = 0;

// requests from other tasks
static volatile int wantDir = 0;
static volatile uint32_t wantAt = 0;
struct Cmd { char op[10]; int n; char key[6]; float v; int ms; };
static QueueHandle_t cmdQ = nullptr;
static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

static uint32_t lastPoll = 0, lastMoveSent = 0;
static bool wasMoving = false;
static int lastDir = 0;
static bool wasOk = true;

static Preferences tokPrefs;
static char token[40] = "";

bool tokenSet() { return token[0] != 0; }
void setToken(const char* t) { strlcpy(token, t, sizeof(token)); tokPrefs.putString("tok", token); }
bool checkToken(const char* t) {
  if (!token[0] || !t) return false;
  size_t n = strlen(token);
  if (strlen(t) != n) return false;
  uint8_t diff = 0;                                   // constant time compare
  for (size_t i = 0; i < n; i++) diff |= token[i] ^ t[i];
  return diff == 0;
}

void init() {
  tokPrefs.begin("deskcfg", false);
  tokPrefs.getString("tok", token, sizeof(token));
  client.begin();
  cmdQ = xQueueCreate(4, sizeof(Cmd));
  st.paired = client.paired();
  if (st.paired) LOGI("desk", "paired with %s (client %s)", client.host().c_str(), client.id().c_str());
}

State state() {
  portENTER_CRITICAL(&mux);
  State s = st;
  portEXIT_CRITICAL(&mux);
  s.ageMs = lastOk ? millis() - lastOk : 0xFFFFFFFF;
  s.paired = client.paired();
  return s;
}

const char* host() { return client.host().c_str(); }

void moveHold(int dir) { wantDir = dir; wantAt = millis(); }

void command(const char* op, int n, const char* key, float v, int ms) {
  if (!cmdQ) return;
  Cmd c = {};
  strlcpy(c.op, op, sizeof(c.op));
  c.n = n; c.v = v; c.ms = ms;
  if (key) strlcpy(c.key, key, sizeof(c.key));
  if (xQueueSend(cmdQ, &c, 0) != pdTRUE) LOGW("desk", "command queue full: %s dropped", op);
}

bool pair(const char* hostName, String& err) {
  LOGI("desk", "pairing with %s ...", hostName);
  bool ok = client.pair(String(hostName), ROBOT_NAME, err);
  if (ok) { LOGI("desk", "paired, client id %s", client.id().c_str()); events::publish(events::EV_DESK, 5); }
  else LOGW("desk", "pairing failed: %s", err.c_str());
  return ok;
}

void unpair() {
  token[0] = 0;
  tokPrefs.remove("tok");
  JsonDocument q, r;
  String err;
  q["op"] = "unpair";
  client.call(q, r, err, 1500);                 // tell the desk too (best effort)
  client.unpair();
  LOGI("desk", "unpaired");
}

static void setErr(const char* e) {
  portENTER_CRITICAL(&mux);
  strlcpy(st.err, e, sizeof(st.err));
  portEXIT_CRITICAL(&mux);
}

static bool send(JsonDocument& q, JsonDocument& r) {
  String err;
  bool ok = client.call(q, r, err, 2500);
  if (!ok) { setErr(err.c_str()); if (wasOk) LOGW("desk", "link problem: %s", err.c_str()); }
  else if (!(r["ok"] | false)) { setErr(r["err"] | "refused"); LOGW("desk", "%s refused: %s", (const char*)(q["op"] | "?"), (const char*)(r["err"] | "?")); }
  return ok;
}

static void poll() {
  JsonDocument q, r;
  q["op"] = "state";
  bool ok = send(q, r);
  portENTER_CRITICAL(&mux);
  st.ok = ok;
  if (ok) {
    st.hv = r["hv"] | false;
    st.h = r["h"] | 0.0f;
    st.moving = r["moving"] | false;
    st.dir = r["dir"] | 0;
    st.armed = r["armed"] | true;
    st.travel = r["travel"] | false;
    strlcpy(st.src, r["src"] | "none", sizeof(st.src));
    st.err[0] = 0;
    lastOk = millis();
  }
  portEXIT_CRITICAL(&mux);
  if (ok != wasOk) {
    wasOk = ok;
    if (ok) LOGI("desk", "link restored"); else events::publish(events::EV_DESK, 4);
  }
  if (!ok) return;
  // events for the robot's personality
  int dir = st.dir;
  bool moving = st.moving || st.dir != 0 || st.travel;
  if (dir != 0 && dir != lastDir) events::publish(events::EV_DESK, dir > 0 ? 1 : 2);
  if (!moving && wasMoving) events::publish(events::EV_DESK, 3);
  wasMoving = moving;
  lastDir = dir;
}

void update() {
  if (!client.paired() || WiFi.status() != WL_CONNECTED) return;
  uint32_t now = millis();
  // 1. held button: keep sending "move" every 250 ms (dead-man: the desk stops when this stops)
  int dir = wantDir;
  if (dir != 0 && now - wantAt < 500) {
    if (now - lastMoveSent >= 250) {
      lastMoveSent = now;
      JsonDocument q, r;
      q["op"] = "move";
      q["dir"] = dir > 0 ? "up" : "down";
      q["ms"] = 700;
      send(q, r);
    }
  }
  // 2. queued commands
  Cmd c;
  if (xQueueReceive(cmdQ, &c, 0) == pdTRUE) {
    JsonDocument q, r;
    q["op"] = c.op;
    if (!strcmp(c.op, "preset") || !strcmp(c.op, "save")) q["n"] = c.n;
    if (!strcmp(c.op, "tap")) { q["key"] = c.key; q["ms"] = c.ms ? c.ms : 150; }
    if (!strcmp(c.op, "setheight")) q["h"] = c.v;
    if (!strcmp(c.op, "arm")) q["on"] = c.n != 0;
    LOGI("desk", "-> %s", c.op);
    send(q, r);
    lastPoll = 0;                                   // refresh the state right away
  }
  // 3. state polling (faster while moving)
  uint32_t every = (st.moving || st.dir != 0 || dir != 0) ? 500 : 2000;
  if (now - lastPoll >= every) { lastPoll = now; poll(); }
}

}  // namespace desk
