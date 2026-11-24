// ESP32-CAM (AI-Thinker, OV2640) firmware for Chiku's hand box.
//  - MJPEG stream  http://<ip>/stream     snapshot http://<ip>/capture
//  - UART headLink to the head (JSON lines, 115200 baud, see src/head/camlink.h)
//  - reads the HC-SR04P sonar and sends {"evt":"dist","cm":N} five times a second
//  - camera sleeps (deinit) when nobody uses it; wakes on the first request
// Wi-Fi credentials are sent by the head ({"cmd":"wifi",...}) and kept in NVS.
// No passwords are stored in this source code.
#include <Arduino.h>
#include <atomic>
#include <WiFi.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include "esp_camera.h"
#include "esp_http_server.h"
#include "pins.h"

// ---- AI-Thinker camera pins ----
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

static const uint32_t CAM_IDLE_MS = 30000;     // camera sleeps after this idle time
static const uint32_t KEEP_AWAKE_MS = 300000;  // "wake" command keeps it on this long

static Preferences prefs;
static HardwareSerial& headLink = Serial2;
static httpd_handle_t httpd = nullptr;
static volatile bool camOn = false;
static volatile uint32_t lastUse = 0;
static volatile uint32_t keepUntil = 0;
static std::atomic<int> streamClients{0};
static SemaphoreHandle_t camMutex;
static char ssid[33] = "", pass[65] = "";
static bool wifiWanted = false;

// ---------------- camera power ----------------
static bool cameraStart() {
  if (camOn) return true;
  camera_config_t c = {};
  c.ledc_channel = LEDC_CHANNEL_0;
  c.ledc_timer = LEDC_TIMER_0;
  c.pin_d0 = Y2_GPIO_NUM; c.pin_d1 = Y3_GPIO_NUM; c.pin_d2 = Y4_GPIO_NUM; c.pin_d3 = Y5_GPIO_NUM;
  c.pin_d4 = Y6_GPIO_NUM; c.pin_d5 = Y7_GPIO_NUM; c.pin_d6 = Y8_GPIO_NUM; c.pin_d7 = Y9_GPIO_NUM;
  c.pin_xclk = XCLK_GPIO_NUM; c.pin_pclk = PCLK_GPIO_NUM;
  c.pin_vsync = VSYNC_GPIO_NUM; c.pin_href = HREF_GPIO_NUM;
  c.pin_sccb_sda = SIOD_GPIO_NUM; c.pin_sccb_scl = SIOC_GPIO_NUM;
  c.pin_pwdn = PWDN_GPIO_NUM; c.pin_reset = RESET_GPIO_NUM;
  c.xclk_freq_hz = 20000000;
  c.pixel_format = PIXFORMAT_JPEG;
  c.frame_size = FRAMESIZE_VGA;
  c.jpeg_quality = 12;
  c.fb_count = 2;
  c.fb_location = CAMERA_FB_IN_PSRAM;
  c.grab_mode = CAMERA_GRAB_LATEST;
  if (esp_camera_init(&c) != ESP_OK) return false;
  camOn = true;
  lastUse = millis();
  headLink.print("{\"evt\":\"cam\",\"state\":\"on\"}\n");
  return true;
}

static void cameraStop() {
  if (!camOn) return;
  esp_camera_deinit();
  camOn = false;
  headLink.print("{\"evt\":\"cam\",\"state\":\"sleep\"}\n");
}

// ---------------- HTTP ----------------
static esp_err_t indexHandler(httpd_req_t* r) {
  static const char page[] =
      "<html><body style='font-family:sans-serif;background:#111;color:#eee'>"
      "<h3>Chiku camera</h3><a style='color:#6cf' href=/stream>/stream</a> &nbsp; "
      "<a style='color:#6cf' href=/capture>/capture</a><br><br><img src=/stream width=480></body></html>";
  httpd_resp_set_type(r, "text/html");
  return httpd_resp_send(r, page, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t captureHandler(httpd_req_t* r) {
  xSemaphoreTake(camMutex, portMAX_DELAY);
  esp_err_t res = ESP_FAIL;
  if (cameraStart()) {
    camera_fb_t* fb = esp_camera_fb_get();
    if (fb) {
      httpd_resp_set_type(r, "image/jpeg");
      httpd_resp_set_hdr(r, "Content-Disposition", "inline; filename=capture.jpg");
      res = httpd_resp_send(r, (const char*)fb->buf, fb->len);
      esp_camera_fb_return(fb);
    }
    lastUse = millis();
  }
  xSemaphoreGive(camMutex);
  if (res != ESP_OK) httpd_resp_send_500(r);
  return res;
}

static esp_err_t streamHandler(httpd_req_t* r) {
  static const char* BOUNDARY = "\r\n--frame\r\n";
  httpd_resp_set_type(r, "multipart/x-mixed-replace;boundary=frame");
  httpd_resp_set_hdr(r, "Access-Control-Allow-Origin", "*");
  streamClients++;
  esp_err_t res = ESP_OK;
  while (res == ESP_OK) {
    xSemaphoreTake(camMutex, portMAX_DELAY);
    camera_fb_t* fb = cameraStart() ? esp_camera_fb_get() : nullptr;
    if (!fb) { xSemaphoreGive(camMutex); res = ESP_FAIL; break; }
    char hdr[64];
    int n = snprintf(hdr, sizeof(hdr), "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", (unsigned)fb->len);
    res = httpd_resp_send_chunk(r, BOUNDARY, strlen(BOUNDARY));
    if (res == ESP_OK) res = httpd_resp_send_chunk(r, hdr, n);
    if (res == ESP_OK) res = httpd_resp_send_chunk(r, (const char*)fb->buf, fb->len);
    esp_camera_fb_return(fb);
    lastUse = millis();
    xSemaphoreGive(camMutex);
    vTaskDelay(1);
  }
  streamClients--;
  return res;
}

static void startServer() {
  httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
  cfg.server_port = 80;
  cfg.max_uri_handlers = 6;
  if (httpd_start(&httpd, &cfg) != ESP_OK) return;
  httpd_uri_t u1 = {"/", HTTP_GET, indexHandler, nullptr};
  httpd_uri_t u2 = {"/capture", HTTP_GET, captureHandler, nullptr};
  httpd_uri_t u3 = {"/stream", HTTP_GET, streamHandler, nullptr};
  httpd_register_uri_handler(httpd, &u1);
  httpd_register_uri_handler(httpd, &u2);
  httpd_register_uri_handler(httpd, &u3);
}

// ---------------- Wi-Fi ----------------
static void wifiConnect() {
  if (!ssid[0]) return;
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(true);
  WiFi.begin(ssid, pass);
  wifiWanted = true;
}

// ---------------- UART headLink ----------------
static void sendIp() {
  char buf[64];
  snprintf(buf, sizeof(buf), "{\"evt\":\"ip\",\"ip\":\"%s\"}\n",
           WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString().c_str() : "");
  headLink.print(buf);
}

static void handleCommand(const char* line) {
  JsonDocument d;
  if (deserializeJson(d, line)) return;
  const char* cmd = d["cmd"] | "";
  if (!strcmp(cmd, "snap")) {
    xSemaphoreTake(camMutex, portMAX_DELAY);
    int len = 0;
    bool ok = false;
    if (cameraStart()) {
      camera_fb_t* fb = esp_camera_fb_get();
      if (fb) { len = fb->len; ok = true; esp_camera_fb_return(fb); }
      lastUse = millis();
    }
    xSemaphoreGive(camMutex);
    char buf[64];
    snprintf(buf, sizeof(buf), "{\"evt\":\"snap\",\"ok\":%s,\"len\":%d}\n", ok ? "true" : "false", len);
    headLink.print(buf);
  } else if (!strcmp(cmd, "led")) {
    digitalWrite(CAM_PIN_FLASH, (d["on"] | false) ? HIGH : LOW);
  } else if (!strcmp(cmd, "wake")) {
    xSemaphoreTake(camMutex, portMAX_DELAY);
    cameraStart();
    xSemaphoreGive(camMutex);
    keepUntil = millis() + KEEP_AWAKE_MS;
  } else if (!strcmp(cmd, "sleep")) {
    keepUntil = 0;
    xSemaphoreTake(camMutex, portMAX_DELAY);
    if (streamClients == 0) cameraStop();
    xSemaphoreGive(camMutex);
  } else if (!strcmp(cmd, "ip")) {
    sendIp();
  } else if (!strcmp(cmd, "wifi")) {
    strlcpy(ssid, d["ssid"] | "", sizeof(ssid));
    strlcpy(pass, d["pass"] | "", sizeof(pass));
    prefs.putString("ssid", ssid);
    prefs.putString("pass", pass);
    WiFi.disconnect();
    wifiConnect();
  }
}

static void readLink() {
  static char line[256];
  static size_t n = 0;
  while (headLink.available()) {
    char c = headLink.read();
    if (c == '\n' || c == '\r') {
      if (n) { line[n] = 0; if (line[0] == '{') handleCommand(line); n = 0; }
    } else if (n < sizeof(line) - 1) line[n++] = c;
    else n = 0;
  }
}

// ---------------- sonar (HC-SR04P, 3.3 V) ----------------
static int readSonarCm() {
  digitalWrite(CAM_PIN_TRIG, LOW); delayMicroseconds(2);
  digitalWrite(CAM_PIN_TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(CAM_PIN_TRIG, LOW);
  unsigned long us = pulseIn(CAM_PIN_ECHO, HIGH, 25000);
  return us ? (int)(us / 58) : -1;
}

void setup() {
  Serial.begin(115200);                       // USB-TTL debug / flashing port
  pinMode(CAM_PIN_FLASH, OUTPUT);
  digitalWrite(CAM_PIN_FLASH, LOW);
  pinMode(CAM_PIN_TRIG, OUTPUT);
  pinMode(CAM_PIN_ECHO, INPUT);
  headLink.setRxBufferSize(512);
  headLink.begin(115200, SERIAL_8N1, CAM_PIN_LINK_RX, CAM_PIN_LINK_TX);
  camMutex = xSemaphoreCreateMutex();
  prefs.begin("cam", false);
  prefs.getString("ssid", ssid, sizeof(ssid));
  prefs.getString("pass", pass, sizeof(pass));
  wifiConnect();
  startServer();
  headLink.print("{\"evt\":\"ready\"}\n");
}

void loop() {
  static uint32_t lastSonar = 0, lastDist = 0, lastIp = 0;
  static bool wasUp = false;
  uint32_t now = millis();
  readLink();

  // sonar: measure every 100 ms, report 5 times a second
  if (now - lastSonar >= 100) {
    lastSonar = now;
    int cm = readSonarCm();
    if (cm > 0 && now - lastDist >= 200) {
      lastDist = now;
      char buf[32];
      snprintf(buf, sizeof(buf), "{\"evt\":\"dist\",\"cm\":%d}\n", cm);
      headLink.print(buf);
    }
  }

  // Wi-Fi state -> head (also works as a heartbeat every 3 s)
  bool up = WiFi.status() == WL_CONNECTED;
  if ((up != wasUp) || (now - lastIp > 3000)) {
    wasUp = up;
    lastIp = now;
    sendIp();
  }
  if (!up && wifiWanted && now % 30000 < 20) WiFi.reconnect();

  // camera sleeps when unused
  if (camOn && streamClients == 0 && now > keepUntil && now - lastUse > CAM_IDLE_MS) {
    xSemaphoreTake(camMutex, portMAX_DELAY);
    cameraStop();
    xSemaphoreGive(camMutex);
  }
  delay(5);
}
