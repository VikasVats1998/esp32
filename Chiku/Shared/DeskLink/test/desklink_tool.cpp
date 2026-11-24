// Native test tool (PC): lets the Node tests in RobotApp check that the app's JavaScript
// crypto and this C++ code agree byte for byte.
//   keygen                              -> privhex pubhex
//   pub <privhex>                       -> pubhex
//   derive <privhex> <peerpub> <clientpub> <serverpub>   -> keyhex
//   seal <keyhex> <challengehex> <purpose> <text>        -> bloghex
//   open <keyhex> <challengehex> <purpose> <bloghex>     -> text   (exit 1 if the tag is wrong)
//   selftest                            -> full round trip, exit 0 when everything works
#include "../src/desklink.h"
#include <iostream>
using namespace desklink;

int main(int argc, char** argv) {
  std::string cmd = argc > 1 ? argv[1] : "";
  if (cmd == "keygen") {
    KeyPair kp;
    if (!generateKeyPair(kp)) return 2;
    std::cout << toHex(kp.priv.data(), 32) << " " << toHex(kp.pub.data(), PUB_LEN) << "\n";
  } else if (cmd == "pub" && argc == 3) {
    std::array<uint8_t, 32> p; Pub pub;
    if (!fromHex(argv[2], p) || !publicFromPrivate(p, pub)) return 2;
    std::cout << toHex(pub.data(), PUB_LEN) << "\n";
  } else if (cmd == "derive" && argc == 6) {
    KeyPair me; Pub peer, cp, sp; Key k;
    if (!fromHex(argv[2], me.priv) || !publicFromPrivate(me.priv, me.pub) || !fromHex(argv[3], peer) || !fromHex(argv[4], cp) || !fromHex(argv[5], sp)) return 2;
    if (!deriveKey(me, peer, cp, sp, k)) return 3;
    std::cout << toHex(k.data(), KEY_LEN) << "\n";
  } else if (cmd == "seal" && argc == 6) {
    Key k; Challenge c;
    if (!fromHex(argv[2], k) || !fromHex(argv[3], c)) return 2;
    std::string b = seal(k, c, argv[4], argv[5]);
    if (b.empty()) return 3;
    std::cout << toHex(b) << "\n";
  } else if (cmd == "open" && argc == 6) {
    Key k; Challenge c; std::string blob, plain;
    if (!fromHex(argv[2], k) || !fromHex(argv[3], c) || !fromHex(argv[5], blob)) return 2;
    if (!open(k, c, argv[4], blob, plain)) { std::cerr << "bad tag\n"; return 1; }
    std::cout << plain << "\n";
  } else if (cmd == "selftest") {
    KeyPair a, b; Key ka, kb;
    bool ok = generateKeyPair(a) && generateKeyPair(b) && deriveKey(a, b.pub, a.pub, b.pub, ka) && deriveKey(b, a.pub, a.pub, b.pub, kb) && ka == kb;
    std::cout << "ecdh agree: " << ok << "\n";
    Challenge c; ChallengeStore store; store.issue(c);
    std::string blob = seal(ka, c, "cmd", "{\"op\":\"state\"}"), plain;
    bool open1 = open(kb, c, "cmd", blob, plain) && plain == "{\"op\":\"state\"}";
    bool wrongPurpose = !open(kb, c, "rsp", blob, plain);
    Challenge other = c; other[0] ^= 1;
    bool wrongChallenge = !open(kb, other, "cmd", blob, plain);
    std::string tampered = blob; tampered[20] ^= 1;
    bool tamper = !open(kb, c, "cmd", tampered, plain);
    bool once = store.consume(c) && !store.consume(c);
    std::cout << "open " << open1 << " purpose-bound " << wrongPurpose << " challenge-bound " << wrongChallenge << " tamper " << tamper << " one-use " << once << "\n";
    return (ok && open1 && wrongPurpose && wrongChallenge && tamper && once) ? 0 : 1;
  } else {
    std::cerr << "usage: see header comment\n";
    return 2;
  }
  return 0;
}
