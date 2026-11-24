// remotelink.h - frame format and security of the gimbal remote link ("RemoteLink v1").
// Portable C++ (builds on a PC for tests). The radio part (ESP-NOW) lives in the two firmwares.
//
//  frame = magic(1) version(1) type(1) session(4) seq(4) payload(n) mac(8)
//  mac   = HMAC-SHA256(key, everything before it), first 8 bytes
//  * session = boot counter of the sender (kept in flash), seq = frame counter inside the session.
//    The receiver accepts a frame only if (session, seq) is newer than the last accepted one: recorded frames
//    can not be replayed, not even after the remote was restarted.
//  * Pairing frames (type PAIR_REQ / PAIR_RSP) carry ECDH public keys and have no MAC (the key does not exist yet).
//    The key = HKDF(ECDH P-256 secret) as in DeskLink; PAIR_RSP carries a confirmation tag so both sides know the keys match.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "desklink.h"

namespace remotelink {

constexpr uint8_t MAGIC = 0xA7;
constexpr uint8_t VERSION = 1;
constexpr size_t HEADER_LEN = 11;
constexpr size_t MAC_LEN = 8;
constexpr size_t MAX_FRAME = 64;

enum Type : uint8_t { T_CONTROL = 1, T_STATUS = 2, T_PAIR_REQ = 3, T_PAIR_RSP = 4 };

// Keys the gimbal module can press (bit mask, "held" semantics: pressed while the bit is set).
enum KeyBit : uint16_t {
  KEY_SHUTTER = 1 << 0,    // photo / start-stop recording
  KEY_M = 1 << 1,          // M (mode) button
  KEY_TRIGGER = 1 << 2,    // trigger on the back
  KEY_ZOOM_IN = 1 << 3,
  KEY_ZOOM_OUT = 1 << 4,
  KEY_X = 1 << 5,          // spare
};

// Macros the gimbal module runs by itself (sequences of key presses, see Gimbal/src/macros.cpp).
enum Macro : uint8_t {
  MACRO_NONE = 0,
  MACRO_PHOTO_VIDEO = 1,   // switch photo / video
  MACRO_SWITCH_CAMERA = 2, // front / rear camera
  MACRO_ACTIVE_TRACK = 3,  // start / stop ActiveTrack
  MACRO_RECENTER = 4,      // recenter the gimbal
  MACRO_FLIP = 5,          // selfie / flip
  MACRO_LOCK = 6,          // lock the current direction
  MACRO_SPORT = 7,         // sport / follow speed
  MACRO_COUNT
};

// Flags in Control::flags
enum ControlFlag : uint8_t {
  F_ACTIVE = 1 << 0,       // the remote wants to drive the sticks now (otherwise the physical joystick stays in charge)
  F_FINE = 1 << 1,         // fine mode is on (informational)
};

struct Control {           // 12 bytes
  int8_t jx, jy;           // joystick -127..127 (after the remote's curve)
  int8_t zoom;             // zoom axis -127..127 (analog zoom, if the gimbal has a zoom slider)
  uint8_t flags;
  uint16_t keys;           // KeyBit mask
  uint8_t macro;           // Macro id, acts when macroCount changes
  uint8_t macroCount;
  uint32_t gsession;       // boot session of the gimbal module (learned from its status frames): frames recorded before a
                           // gimbal restart are worthless afterwards
};

enum StatusFlag : uint8_t { S_FAILSAFE = 1 << 0, S_MACRO_RUNNING = 1 << 1, S_LEARNING = 1 << 2, S_INJECTING = 1 << 3 };
struct Status {            // 4 bytes
  uint8_t flags;
  uint8_t battPct;         // gimbal rail estimate, 255 = unknown
  int8_t rssi;
  uint8_t macroDone;       // echo of the last finished macroCount
};

struct Header {
  Type type;
  uint32_t session;
  uint32_t seq;
};

// ---- frames ----
size_t encode(const desklink::Key& key, Type type, uint32_t session, uint32_t seq, const uint8_t* payload, size_t n, uint8_t* out);
// Verifies magic, version and (for authenticated types) the MAC. Payload is copied to `payload` (max MAX_FRAME).
bool decode(const desklink::Key& key, const uint8_t* in, size_t len, Header& h, uint8_t* payload, size_t& n);

// Pairing frames have no MAC.
size_t encodePair(Type type, const uint8_t* payload, size_t n, uint8_t* out);

// ---- replay protection ----
class ReplayGuard {
 public:
  void load(uint32_t lastSession) { session_ = lastSession; seq_ = 0; }
  bool accept(uint32_t session, uint32_t seq);       // true if newer than everything accepted before
  uint32_t session() const { return session_; }
 private:
  uint32_t session_ = 0, seq_ = 0;
};

// ---- pairing confirmation tag (8 bytes) ----
void confirmTag(const desklink::Key& key, const desklink::Pub& clientPub, const desklink::Pub& serverPub, uint8_t out[8]);

// ---- payload helpers (little endian, packed) ----
size_t pack(const Control& c, uint8_t* out);
bool unpack(const uint8_t* in, size_t n, Control& c);
size_t pack(const Status& s, uint8_t* out);
bool unpack(const uint8_t* in, size_t n, Status& s);

}  // namespace remotelink
