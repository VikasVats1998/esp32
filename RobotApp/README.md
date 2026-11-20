# Robot app (Chiku and Jinu)

React Native app for testing and controlling both robots by hand: switch modes, show faces and feelings,
move servos, test sensors and touch pads live, simulate events, change settings, upload voice clips and firmware.

Stack (latest at the time of writing): **Expo SDK 58**, React Native 0.88, React 19, TypeScript, Expo Router
(native tabs), TanStack Query (live polling), Zustand (saved robots), React Compiler lint rules.
Works in **Expo Go** (no custom native code).

## Run

```bash
npm install
npx expo start            # scan the QR code with Expo Go (phone on the same Wi-Fi as the robots)
```

1. **Robots**: *Scan Wi-Fi network* (asks every address of your subnet), or add `chiku.local` / `jinu.local` / an IP.
2. **Control**: mode (Auto / Manual / Explore), mute, faces, feelings, moves, voice clips, look pad, Chiku screens.
3. **Test** (four views):
   * **Live**: touch pads lighting up, sonar bar, IMU, hardware status, display test patterns, sound test.
   * **Checklist**: one test for **every** screen, touch pad, sensor, motor and sound (about 40 tests). Automatic tests decide
     pass/fail themselves; "you act" tests wait for your touch or hand; "you confirm" tests make the robot move/play/show
     something and ask what you saw or heard. Summary, share-as-text report.
   * **Servos**: slider per servo, trim / min / max / reverse, save to the robot.
   * **Simulate**: fake any pad gesture or sensor event without touching the robot.
4. **Console**:
   * **Commands**: type `mood happy`, `move dance`, `servo 1 100`, `pad 4 double`, or any raw request (`POST /api/beep?hz=440&ms=300`). Type `help`.
   * **Robot log**: the robot's complete log (its own messages, every request it received, events, ESP system logs), with level filter, search, pause, share.
   * **Events**: touch gestures, shakes, buttons as the robot saw them.
   * **Requests**: every request the app sent, with status, time and answer (and every failure).
5. **Desk** (Control, Desk): the standing-desk accessory: secure pairing, live height, hold-to-move UP/DOWN, STOP, memory positions, child lock, pairing the robots, diagnostics. The desk tests are in Test, Checklist. Console accepts `desk ...` commands.
6. **Settings**: brightness, volume, (Chiku) weather location, alarms, timers, camera, voice upload, firmware (.bin) upload, reboot.

**Feedback on every command:** a green/red line at the top of every screen shows what the robot answered (and how long it took),
including the robot's own message. Errors also appear in Console > Requests and Console > Robot log.

Use **Manual** mode while testing hardware: the robot then only does what you tell it.

## Develop without a robot

```bash
node tools/mock-robot.mjs                       # Chiku on :8787, Jinu on :8788
npx tsx tools/mock-desk.mts                      # mock desk module on :8790 (real DeskLink protocol)
npx tsx tools/smoke-desk.mts localhost:8790     # desk client: pairing, sealed commands, dead-man, replay, lock-out
npx tsx tools/smoke.mts localhost:8787 chiku    # ~50 checks: API, feedback, log, console commands, checklist (also works against a real robot)
```
In a dev build the Robots tab has "Mock" buttons (Android emulator uses `10.0.2.2:8787`).

## Checks

```bash
npx tsc --noEmit && npx eslint src tools && npx expo-doctor
npx expo export --platform android --platform ios   # bundles both platforms
```

## Notes

* Robots serve plain `http` on the LAN. `app.json` allows this (iOS local networking, Android cleartext).
* The API is documented in [docs/API.md](docs/API.md). The app hides features a robot does not have (`caps`).
* Not tested on a physical phone or against real robot hardware when written; the client was tested against the mock server.
