// touch.cpp - see touch.h
#include "touch.h"
#include "config.h"
#include "bus.h"
#include "events.h"
#include "display.h"
#include <lvgl.h>
#include <TouchDrvCSTXXX.hpp>

namespace touch {

static TouchDrvCST92xx cst;
static volatile bool irqFlag = false;
static volatile bool down = false;
static touch::GestureMode mode = touch::G_ALL;
static bool ready = false;

// Gesture tracking
static int16_t startX, startY, lastX, lastY;
static uint32_t startMs;
static bool longSent, moved;

static void IRAM_ATTR onIrq() { irqFlag = true; }

static void gestureStep(bool pressed, int16_t x, int16_t y) {
  uint32_t now = millis();
  if (pressed && !down) {                       // finger just landed
    startX = lastX = x; startY = lastY = y;
    startMs = now; longSent = false; moved = false;
  } else if (pressed) {                         // finger moving
    lastX = x; lastY = y;
    if (abs(x - startX) > 25 || abs(y - startY) > 25) moved = true;
    if (!longSent && !moved && now - startMs > POWER_MENU_HOLD_MS) {
      longSent = true;
      if (mode == G_ALL) events::publish(events::EV_TOUCH_LONG, x, y);
    }
  } else if (down) {                            // finger just left
    int dx = lastX - startX, dy = lastY - startY;
    bool tap = abs(dx) < 30 && abs(dy) < 30 && now - startMs < 600;
    if (longSent) {
      // nothing: the long press was already reported
    } else if (mode != G_NONE && abs(dx) > 70 && abs(dx) > abs(dy) * 1.5f) {
      events::publish(events::EV_SWIPE, dx > 0 ? 1 : -1, 0);
    } else if (mode == G_ALL && abs(dy) > 70 && abs(dy) > abs(dx) * 1.5f) {
      events::publish(events::EV_SWIPE, 0, dy > 0 ? 1 : -1);
    } else if (tap) {
      events::publish(events::EV_TOUCH_TAP, lastX, lastY);
    }
  }
  down = pressed;
}

static void readCb(lv_indev_t*, lv_indev_data_t* data) {
  static int16_t px = 0, py = 0;
  bool pressed = false;
  if (ready && (irqFlag || down)) {
    irqFlag = false;
    int16_t x[2], y[2];
    bus::lock();
    uint8_t n = cst.getPoint(x, y, 1);
    bus::unlock();
    if (n > 0) {
      px = x[0]; py = y[0];
      display::toLogical(px, py);             // panel -> what you see (screen rotation)
      pressed = true;
    }
  }
  gestureStep(pressed, px, py);
  data->point.x = px;
  data->point.y = py;
  data->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

bool init() {
  pinMode(PIN_TP_INT, INPUT_PULLUP);
  pinMode(PIN_TP_RST, OUTPUT);
  digitalWrite(PIN_TP_RST, LOW); delay(30);
  digitalWrite(PIN_TP_RST, HIGH); delay(60);

  cst.setPins(PIN_TP_RST, PIN_TP_INT);
  bus::lock();
  ready = cst.begin(Wire, I2C_ADDR_CST9217, PIN_I2C0_SDA, PIN_I2C0_SCL);
  if (ready) {
    cst.setMaxCoordinates(LCD_W, LCD_H);
    cst.setMirrorXY(true, true);
  }
  bus::unlock();
  if (!ready) return false;
  attachInterrupt(digitalPinToInterrupt(PIN_TP_INT), onIrq, FALLING);

  lv_indev_t* in = lv_indev_create();
  lv_indev_set_type(in, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(in, readCb);
  return true;
}

bool isDown() { return down; }
uint32_t holdMs() { return (down && !moved && !longSent) ? millis() - startMs : 0; }
void setGestures(GestureMode m) { mode = m; }

}  // namespace touch
