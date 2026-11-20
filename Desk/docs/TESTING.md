# Desk module test plan

Everything below can be done from the phone app: **Test, Checklist** (Desk group), **Control, Desk**, **Console** (`desk ...` commands and the desk log).

## Without the desk (on the bench, nothing connected to the handset)

1. Flash the module, pair the phone. Run the Desk checklist "automatic" tests: reachable, secure link, replay / wrong key rejected, height, child lock.
2. LED on a photorelay (or a multimeter on the GPIO): `desk tap x 500` lights output X; `desk up` pulses UP for 0.5 s. Check **UP and DOWN never light together**.
3. Hold UP in the app then turn the phone's Wi-Fi off: the output must switch off within 0.5 s (dead-man).
4. Hold UP for 30 s: it must stop at 25 s.

## On the handset (desk unplugged from the mains while soldering!)

1. Solder one key at a time; test that key with `desk tap <key> 150` (X and S first, they are harmless).
2. Plug the desk in. **Nudge UP / DOWN** (checklist): the desk moves about 1 cm and stops by itself.
3. **STOP** test, then each **memory position** (only works if memories were saved with the handset).
4. Height: compare the app value with the handset display; calibrate if it says "estimate".
5. Robots: Control, Desk, Robots: pair, then the robot screen (Chiku long-press menu) and the app's robot desk buttons.

## Security checks

* Checklist "Replay and wrong key are rejected" must pass.
* After pairing everything, power-cycle, wait 3.5 minutes, then try pairing a new device: it must be refused (HTTP 403).
* Clients list (Control, Desk, Diagnostics) shows only your devices.

## Development without hardware

```bash
cd RobotApp
npx tsx tools/mock-desk.mts &                  # mock desk module on :8790 (same protocol and safety rules)
npx tsx tools/smoke-desk.mts localhost:8790    # ~40 checks: pairing, sealed commands, dead-man, child lock, replay, wrong key, lock-out
Shared/DeskLink/test/run.sh                    # C++ <-> TypeScript crypto interop
```
