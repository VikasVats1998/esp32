// Native tests (PC) for the RemoteLink frames and the pairing agreement.
//   Shared/RemoteLink/test/run.sh
#include "../src/remotelink.h"
#include <cstring>
#include <iostream>
using namespace remotelink;
static int failed = 0;
#define CHECK(name, cond) do { bool ok_ = (cond); std::cout << (ok_ ? "PASS  " : "FAIL  ") << name << "\n"; if (!ok_) failed++; } while (0)

int main() {
  desklink::KeyPair remote, gimbal;
  desklink::Key kr, kg;
  bool paired = desklink::generateKeyPair(remote) && desklink::generateKeyPair(gimbal) &&
                desklink::deriveKey(remote, gimbal.pub, remote.pub, gimbal.pub, kr) &&
                desklink::deriveKey(gimbal, remote.pub, remote.pub, gimbal.pub, kg);
  CHECK("pairing: both sides derive the same key", paired && kr == kg);
  uint8_t t1[8], t2[8];
  confirmTag(kr, remote.pub, gimbal.pub, t1);
  confirmTag(kg, remote.pub, gimbal.pub, t2);
  CHECK("pairing: confirmation tags match", memcmp(t1, t2, 8) == 0);
  desklink::Key other = kr; other[3] ^= 1;
  confirmTag(other, remote.pub, gimbal.pub, t2);
  CHECK("pairing: a different key gives a different tag", memcmp(t1, t2, 8) != 0);

  Control c = {-100, 90, 5, F_ACTIVE, KEY_SHUTTER | KEY_ZOOM_IN, MACRO_RECENTER, 7, 0xCAFE0001u}, c2 = {};
  uint8_t payload[12], frame[MAX_FRAME], out[MAX_FRAME];
  size_t pl = pack(c, payload);
  size_t fl = encode(kr, T_CONTROL, 5, 100, payload, pl, frame);
  Header h; size_t n = 0;
  CHECK("encode/decode round trip", fl == HEADER_LEN + 12 + MAC_LEN && decode(kg, frame, fl, h, out, n) && h.type == T_CONTROL && h.session == 5 && h.seq == 100 && unpack(out, n, c2) &&
        c2.jx == -100 && c2.jy == 90 && c2.zoom == 5 && c2.keys == (KEY_SHUTTER | KEY_ZOOM_IN) && c2.macro == MACRO_RECENTER && c2.macroCount == 7 && c2.gsession == 0xCAFE0001u);

  uint8_t bad[MAX_FRAME]; memcpy(bad, frame, fl); bad[HEADER_LEN + 1] ^= 0x40;
  CHECK("tampered payload rejected", !decode(kg, bad, fl, h, out, n));
  memcpy(bad, frame, fl); bad[fl - 1] ^= 1;
  CHECK("tampered MAC rejected", !decode(kg, bad, fl, h, out, n));
  memcpy(bad, frame, fl); bad[7] ^= 1;
  CHECK("changed sequence number rejected", !decode(kg, bad, fl, h, out, n));
  CHECK("wrong key rejected", !decode(other, frame, fl, h, out, n));
  CHECK("truncated frame rejected", !decode(kg, frame, fl - 3, h, out, n));
  frame[0] ^= 1;
  CHECK("wrong magic rejected", !decode(kg, frame, fl, h, out, n));
  frame[0] ^= 1;

  ReplayGuard g;
  g.load(4);
  CHECK("replay guard: newer session accepted", g.accept(5, 1));
  CHECK("replay guard: next seq accepted", g.accept(5, 2));
  CHECK("replay guard: same seq rejected (replay)", !g.accept(5, 2));
  CHECK("replay guard: older seq rejected", !g.accept(5, 1));
  CHECK("replay guard: older session rejected", !g.accept(4, 999));
  CHECK("replay guard: restarted remote (session+1) accepted", g.accept(6, 1));
  CHECK("replay guard: old session frames rejected after restart", !g.accept(5, 50));

  uint8_t pub[65] = {4}; uint8_t pf[200];
  size_t pn = encodePair(T_PAIR_REQ, pub, 65, pf);
  CHECK("pairing frame has no MAC and decodes", pn == HEADER_LEN + 65 && decode(kg, pf, pn, h, out, n) && h.type == T_PAIR_REQ && n == 65);

  Status s = {S_INJECTING, 80, -55, 3}, s2 = {};
  pl = pack(s, payload);
  fl = encode(kg, T_STATUS, 1, 1, payload, pl, frame);
  CHECK("status frame round trip", decode(kr, frame, fl, h, out, n) && unpack(out, n, s2) && s2.battPct == 80 && s2.rssi == -55 && s2.macroDone == 3);

  std::cout << (failed ? "\nFAILED\n" : "\nall passed\n");
  return failed ? 1 : 0;
}
