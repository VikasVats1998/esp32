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

1. **Robots** tab: *Scan Wi-Fi network* (asks every address of your subnet), or add `chiku.local` / `jinu.local` / an IP.
2. **Control**: mode (Auto / Manual / Explore), faces, feelings, moves, voice, look pad.
3. **Test**: live touch pads, sonar bar, IMU, display test patterns, sound test, simulated touches and events, event log.
4. **Servos**: slider per servo, trim / min / max / reverse, save to the robot.
5. **Settings**: brightness, volume, (Chiku) weather location, alarms, timers, camera, voice upload, firmware (.bin) upload, reboot.

Use **Manual** mode while testing hardware: the robot then only does what you tell it.

## Develop without a robot

```bash
node tools/mock-robot.mjs                       # Chiku on :8787, Jinu on :8788
npx tsx tools/smoke.mts localhost:8787 chiku    # API client smoke test (also works against a real robot)
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
