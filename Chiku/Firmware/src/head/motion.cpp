// motion.cpp - see motion.h
#include "motion.h"
#include "config.h"
#include "logbuf.h"
#include "events.h"
#include "servo.h"
#include <math.h>

namespace motion {

// Oscillator order: LEFT_HIP, RIGHT_HIP, LEFT_ANKLE, RIGHT_ANKLE, HEAD_PAN.
// Pan: if amp and offset are both 0 the gait leaves the head alone (it keeps
// following the look direction). Otherwise pan = 90 + off + amp*sin(...).
struct GaitDef {
  const char* name;
  float amp[5];     // degrees
  float off[5];     // degrees around 90
  float phase[5];   // degrees
  uint16_t period;  // ms per cycle
  uint16_t ramp;    // ms to fade the move in (and out)
  uint8_t flags;    // F_* below
};
enum { F_HOLD_END = 1, F_RELEASE_END = 2 };   // keep the pose at the end / then switch servos off

// Emotion rows follow the UI kit pose table (page 3): neck, hips, feet in degrees around 90.
// "80<->100" = amp 10; "70<->90" = off -10, amp 10. Tune on your build.
static const GaitDef GAITS[G_COUNT] = {
  //  name        amp (Lhip Rhip Lft Rft neck)  off                    phase                   period ramp
  {"home",      { 0,  0,  0,  0,  0}, { 0,  0,  0,  0,  0}, {  0,   0,   0,   0,   0},  600, 250, 0},
  {"walk",      {28, 28, 18, 18,  0}, { 0,  0,  4, -4,  0}, {  0,   0,  90,  90,   0},  900, 250, 0},
  {"back",      {28, 28, 18, 18,  0}, { 0,  0,  4, -4,  0}, {  0,   0, -90, -90,   0},  900, 250, 0},
  {"left",      {10, 30, 18, 18,  0}, { 0,  0,  4, -4,  0}, {  0,   0,  90,  90,   0},  900, 250, 0},
  {"right",     {30, 10, 18, 18,  0}, { 0,  0,  4, -4,  0}, {  0,   0,  90,  90,   0},  900, 250, 0},
  {"bend",      { 0,  0,  0,  0,  0}, { 0,  0, 38, 38,  0}, {  0,   0,   0,   0,   0}, 1400, 250, 0},
  {"shakeleg",  { 0, 25,  0,  0,  0}, { 0,  0, 35, 35,  0}, {  0,   0,   0,   0,   0},  400, 250, 0},
  {"moonwalk",  {20, 20, 22, 22,  0}, { 0,  0, 10,-10,  0}, {  0, 180, -75,  75,   0}, 1000, 250, 0},
  {"dance",     {25, 25, 30, 30, 30}, { 0,  0,  0,  0,  0}, {  0, 180,  90, 270,   0},  700, 250, 0},
  {"jump",      { 0,  0, 35, 35,  0}, { 0,  0,  0,  0,  0}, {  0,   0,   0,   0,   0},  500, 250, 0},
  {"wiggle",    { 0,  0, 18, 18,  0}, { 0,  0,  0,  0,  0}, {  0,   0,   0, 180,   0},  350, 250, 0},
  // ---- emotions ----
  {"happy",     { 0,  0, 10, 10, 10}, { 0,  0,-10, 10,  0}, {  0,   0,   0, 180,   0},  450, 150, 0},  // nods + feet bounce
  {"excited",   { 0,  0, 30, 30,  5}, { 0,  0,  0,  0,  0}, {  0,   0,   0, 180,   0},  300, 100, 0},  // hop, very fast
  {"angry",     { 0,  0, 15,  0, 10}, { 0,  0,-15,  0,  0}, {  0,   0,   0,   0,  90},  280, 100, 0},  // head shake + left stomp
  {"scared",    { 0,  0,  4,  4,  0}, {-8, -8,  0,  0,-15}, {  0,   0,   0,  90,   0},  120, 150, 0},  // lean back, tremble
  {"sulk",      { 0,  0,  0,  0,  0}, { 0,  0, 16, 16,-55}, {  0,   0,   0,   0,   0}, 1500, 250, 0},  // turn away (ruthna)
  {"jealous",   { 0,  0,  0, 12,  0}, { 0,  0,  6,  0, 42}, {  0,   0,   0,   0,   0},  300, 250, 0},  // side glance, foot tap
  {"shy",       { 0,  0,  0,  0,  6}, { 0,  0, 18, 18,-32}, {  0,   0,   0,   0,   0}, 1100, 250, 0},  // look away, small
  {"love",      {10, 10,  0,  0,  0}, { 0,  0,  0,  0, 15}, {  0, 180,   0,   0,   0}, 1500, 300, 0},  // side sway, slow
  {"sad",       { 0,  0,  0,  0,  0}, { 5, -5,  0,  0,-25}, {  0,   0,   0,   0,   0}, 1200,1200, 0},  // head drops slowly
  {"surprised", { 0,  0,  0,  0,  0}, {20,-20,  0,  0, 10}, {  0,   0,   0,   0,   0},  600,  60, 0},  // jump back, instant
  {"tiltl",     { 0,  0,  0,  0,  0}, { 0,  0, 12, 12,-28}, {  0,   0,   0,   0,   0}, 1200, 250, 0},  // curious tilt
  {"tiltr",     { 0,  0,  0,  0,  0}, { 0,  0,-12,-12, 28}, {  0,   0,   0,   0,   0}, 1200, 250, 0},
  {"sleepy",    { 0,  0,  0,  0,  0}, { 0,  0,  0,  0,-20}, {  0,   0,   0,   0,   0}, 3000,3000, F_HOLD_END | F_RELEASE_END},  // droop, torque off
  {"curious",   { 0,  0,  0,  0,  0}, {-10,10,  5, -5, 20}, {  0,   0,   0,   0,   0}, 1500, 300, 0},  // lean forward + tilt
  {"confused",  { 0,  0,  0,  0, 15}, { 0,  0,  0,  0,  0}, {  0,   0,   0,   0,   0}, 1000, 200, 0},  // tilt L/R
  {"bored",     { 0,  0,7.5f, 0, 20}, { 0,  0,-7.5f,0,  0}, {  0,   0,   0,   0,   0}, 3000, 600, 0},  // slow look, foot tap
  {"dizzy",     {10, 10,  5,  5,  5}, { 0,  0,  0,  0,  0}, {  0, 180,  90, 270,  45},  700, 200, 0},  // phase-shifted wobble
  {"wink",      { 0,  0,  0,  0,  0}, { 0,  0,  0,  0, 10}, {  0,   0,   0,   0,   0},  600, 150, 0},  // quick tilt and back
  {"thinking",  { 0,  0,  5,  0,  0}, { 0,  0, -5,  0, 10}, {  0,   0,   0,   0,   0}, 1200, 300, 0},  // head up, slow foot tap
  {"listen",    { 0,  0,  0,  0,  0}, { 0,  0,  0,  0,  5}, {  0,   0,   0,   0,   0}, 1000, 300, 0},  // lean toward user
  {"speak",     { 0,  0,  0,  0,  3}, { 0,  0,  0,  0,  1}, {  0,   0,   0,   0,   0},  400, 150, 0},  // micro-nod
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
static float lastPan = -1;
static bool followLook = true;
static float speedK = 1.f;
static bool breathe = false;

void init() { q = events::subscribe(12); }

void start(Gait g, uint8_t cycles) {
  if (g >= G_COUNT) g = G_HOME;
  reqGait = g;
  reqCycles = cycles;
  reqFlag = true;
}
void stop() { reqGait = G_HOME; reqCycles = 1; reqFlag = true; }
bool busy() { return running; }
Gait current() { return gait; }
void setAvoidMode(bool on) {
  LOGI("motion", "explore mode %s", on ? "on" : "off"); avoid = on; avoidStep = 0; if (!on) stop(); }
bool avoidMode() { return avoid; }
void setFollowLook(bool on) { followLook = on; }
void lookPan(float x) { panTarget = constrain(90.f + x * LOOK_NECK_DEG, 20.f, 160.f); }
void setSpeedScale(float k) { speedK = constrain(k, 0.3f, 1.5f); }
void setIdleBreath(bool on) { breathe = on && IDLE_BREATHE_DEG > 0; }

static void begin(Gait g, uint8_t cycles) {
  LOGI("motion", "start %s x%u", GAITS[g].name, cycles);
  gait = g;
  t = 0;
  totalMs = GAITS[g].period / speedK * (cycles ? cycles : 1);
  running = true;
  if (g == G_HOME) {                       // glide home with easing
    for (int i = 0; i < 4; i++) servo::setTarget(HW[i], 90, 120);
    running = false;
  }
}

static void endGait() {
  LOGI("motion", "end %s", GAITS[gait].name);
  running = false;
  uint8_t f = GAITS[gait].flags;
  if (f & F_RELEASE_END) { servo::releaseAll(); return; }
  if (f & F_HOLD_END) return;
  for (int i = 0; i < 4; i++) servo::setTarget(HW[i], 90, 150);
  if (GAITS[gait].amp[4] != 0 || GAITS[gait].off[4] != 0) panTarget = 90;   // head back to the middle
}

static void handleEvents() {
  events::Event e;
  while (xQueueReceive(q, &e, 0) == pdTRUE) {
    switch (e.type) {
      case events::EV_FALL:
        LOGW("motion", "FALL: servos released");
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
        if (followLook) lookPan(e.a / 100.f);
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
      LOGW("motion", "obstacle at %d cm: stop walking", lastDist);
      running = false; endGait(); avoidUntil = now + 600;
    } else if (t >= totalMs) {
      endGait();
    } else {
      // Fade the movement in and out so it starts and stops softly (hold poses only fade in)
      const GaitDef& d = GAITS[gait];
      float rIn = t / d.ramp, rOut = (d.flags & F_HOLD_END) ? 1.f : (totalMs - t) / d.ramp;
      float ramp = fminf(1.f, fminf(rIn, rOut));
      float period = d.period / speedK;
      for (int i = 0; i < 4; i++) {
        float ph = 2.f * PI * (t / period) + d.phase[i] * DEG_TO_RAD;
        float a = 90.f + (d.off[i] + d.amp[i] * sinf(ph)) * ramp;
        servo::setTargetNow(HW[i], a);
      }
      if (d.amp[4] != 0 || d.off[4] != 0) {          // this gait also moves the head
        float ph = 2.f * PI * (t / period) + d.phase[4] * DEG_TO_RAD;
        float a = 90.f + (d.off[4] + d.amp[4] * sinf(ph)) * ramp;
        servo::setTargetNow(SERVO_HEAD_PAN, a);
        lastPan = a; panTarget = a;                  // so the follow-look code does not fight it
      }
    }
    idleSince = now;
  }
  // Head pan always follows its target gently; while idle it "breathes" (UI kit: +-3 deg, 4 s)
  float want = panTarget + ((breathe && !running) ? IDLE_BREATHE_DEG * sinf(2.f * PI * (now % 4000) / 4000.f) : 0.f);
  if (fabsf(want - lastPan) > 2.f) { servo::setTarget(SERVO_HEAD_PAN, want, 140); lastPan = want; }
  else if (breathe && !running && fabsf(want - lastPan) > 0.3f) { servo::setTargetNow(SERVO_HEAD_PAN, want); lastPan = want; }
}

}  // namespace motion
