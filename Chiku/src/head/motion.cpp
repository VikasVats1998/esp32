// motion.cpp - see motion.h
#include "motion.h"
#include "config.h"
#include "events.h"
#include "servo.h"
#include <math.h>

namespace motion {

// Oscillator order: LEFT_HIP, RIGHT_HIP, LEFT_ANKLE, RIGHT_ANKLE
struct GaitDef {
  const char* name;
  float amp[4];     // degrees
  float off[4];     // degrees around 90
  float phase[4];   // degrees
  uint16_t period;  // ms per cycle
};

static const GaitDef GAITS[G_COUNT] = {
  {"home",      { 0,  0,  0,  0}, { 0,  0,  0,  0}, {  0,   0,   0,   0}, 600},
  {"walk",      {28, 28, 18, 18}, { 0,  0,  4, -4}, {  0,   0,  90,  90}, 900},
  {"back",      {28, 28, 18, 18}, { 0,  0,  4, -4}, {  0,   0, -90, -90}, 900},
  {"left",      {10, 30, 18, 18}, { 0,  0,  4, -4}, {  0,   0,  90,  90}, 900},
  {"right",     {30, 10, 18, 18}, { 0,  0,  4, -4}, {  0,   0,  90,  90}, 900},
  {"bend",      { 0,  0,  0,  0}, { 0,  0, 38,  38}, {  0,   0,   0,   0}, 1400},
  {"shakeleg",  { 0, 25,  0,  0}, { 0,  0, 35,  35}, {  0,   0,   0,   0}, 400},
  {"moonwalk",  {20, 20, 22, 22}, { 0,  0, 10, -10}, {  0, 180, -75,  75}, 1000},
  {"dance",     {25, 25, 30, 30}, { 0,  0,  0,   0}, {  0, 180,  90, 270}, 700},
  {"jump",      { 0,  0, 35, 35}, { 0,  0,  0,   0}, {  0,   0,   0,   0}, 500},
  {"wiggle",    { 0,  0, 18, 18}, { 0,  0,  0,   0}, {  0,   0,   0, 180}, 350},
};
static const uint8_t HW[4] = {SERVO_LEFT_HIP, SERVO_RIGHT_HIP, SERVO_LEFT_ANKLE, SERVO_RIGHT_ANKLE};

const char* gaitName(Gait g) { return g < G_COUNT ? GAITS[g].name : "?"; }
bool gaitFromName(const char* n, Gait& out) {
  for (int i = 0; i < G_COUNT; i++) if (!strcmp(n, GAITS[i].name)) { out = (Gait)i; return true; }
  return false;
}

static QueueHandle_t q = nullptr;
static volatile Gait reqGait = G_HOME;
static volatile uint8_t reqCycles = 0;
static volatile bool reqFlag = false;

static Gait gait = G_HOME;
static bool running = false;
static float t = 0;              // ms inside the gait
static float totalMs = 0;
static bool avoid = false;
static int lastDist = 999;
static uint32_t avoidStep = 0;   // small state machine for avoid mode
static uint32_t avoidUntil = 0;
static uint32_t idleSince = 0;
static float panTarget = 90;

void init() { q = events::subscribe(12); }

void start(Gait g, uint8_t cycles) { reqGait = g; reqCycles = cycles; reqFlag = true; }
void stop() { reqGait = G_HOME; reqCycles = 1; reqFlag = true; }
bool busy() { return running; }
Gait current() { return gait; }
void setAvoidMode(bool on) { avoid = on; avoidStep = 0; if (!on) stop(); }
bool avoidMode() { return avoid; }
void lookPan(float x) { panTarget = constrain(90.f + x * 35.f, 20.f, 160.f); }

static void begin(Gait g, uint8_t cycles) {
  gait = g;
  t = 0;
  totalMs = GAITS[g].period * (cycles ? cycles : 1);
  running = true;
  if (g == G_HOME) {                       // glide home with easing
    for (int i = 0; i < 4; i++) servo::setTarget(HW[i], 90, 120);
    running = false;
  }
}

static void endGait() {
  running = false;
  for (int i = 0; i < 4; i++) servo::setTarget(HW[i], 90, 150);
}

static void handleEvents() {
  events::Event e;
  while (xQueueReceive(q, &e, 0) == pdTRUE) {
    switch (e.type) {
      case events::EV_FALL:
        avoid = false;
        running = false;
        servo::freeze();                   // stop servos, release
        break;
      case events::EV_DIST:
        lastDist = e.a;
        break;
      case events::EV_OBSTACLE:
        if (running && gait == G_WALK) { running = false; endGait(); }
        break;
      case events::EV_MOTION_CMD:
        start((Gait)constrain(e.a, 0, G_COUNT - 1), e.b > 0 ? e.b : 4);
        break;
      case events::EV_LOOK:
        lookPan(e.a / 100.f);
        break;
      default: break;
    }
  }
}

// Avoid mode: walk; if blocked, step back and turn, then continue.
static void avoidLogic(uint32_t now) {
  if (running || now < avoidUntil) return;
  switch (avoidStep) {
    case 0:
      if (lastDist > 0 && lastDist < 18) { avoidStep = 1; }
      else start(G_WALK, 2);
      break;
    case 1: start(G_BACK, 2); avoidStep = 2; break;
    case 2: start((random(0, 2) ? G_TURN_L : G_TURN_R), 3); avoidStep = 0; break;
  }
}

void update(uint32_t dtMs) {
  uint32_t now = millis();
  handleEvents();
  if (reqFlag) {
    reqFlag = false;
    if (reqGait == G_HOME) { running = false; endGait(); }
    else begin(reqGait, reqCycles);
  }
  if (avoid) avoidLogic(now);

  if (running) {
    t += dtMs;
    // Safety: walking forward and something is too close
    if (gait == G_WALK && lastDist > 0 && lastDist < OBSTACLE_STOP_CM) {
      running = false; endGait(); avoidUntil = now + 600;
    } else if (t >= totalMs) {
      endGait();
    } else {
      // Fade the movement in and out over 250 ms so it starts and stops softly
      float ramp = fminf(1.f, fminf(t, totalMs - t) / 250.f);
      const GaitDef& d = GAITS[gait];
      for (int i = 0; i < 4; i++) {
        float ph = 2.f * PI * (t / d.period) + d.phase[i] * DEG_TO_RAD;
        float a = 90.f + (d.off[i] + d.amp[i] * sinf(ph)) * ramp;
        // Mirror the right side so "forward" is the same for both legs
        if (i == 1 || i == 3) a = 90.f - (a - 90.f);
        servo::setTargetNow(HW[i], a);
      }
      if (gait == G_DANCE) panTarget = 90.f + 30.f * sinf(2.f * PI * t / d.period);
    }
    idleSince = now;
  }
  // Head pan always follows its target gently
  static float lastPan = -1;
  if (fabsf(panTarget - lastPan) > 1.f) {
    servo::setTarget(SERVO_HEAD_PAN, panTarget, 140);
    lastPan = panTarget;
  }
}

}  // namespace motion
