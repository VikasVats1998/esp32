# Desk module (standing desk accessory)

Small add-on that sits **inside the standing-desk handset** (ESP32-S3 Super Mini), presses the handset's buttons electrically, reads back the desk height and is controlled
securely by **Chiku**, **Jinu** and the **phone app**.

Docs: [WIRING.md](WIRING.md) (read first) · [SECURITY.md](SECURITY.md) · [TESTING.md](TESTING.md)

```
 phone app ----\                         (Wi-Fi, encrypted DeskLink)
 Chiku --------+--------------->  Desk module  --photorelays-->  handset buttons (UP, DOWN, 1-4, S, X)
 Jinu ---------/                     ^  (ESP32-S3 Super Mini)
                                     +-- height feedback: handset data line (read only) or estimate
```

## Build and flash

```bash
pip install platformio "click<8.2"
pio run -e desk                 # build
pio run -e desk -t upload       # flash over USB-C (hold BOOT while plugging in)
```

## First setup

1. Wire it (WIRING.md), flash it, join `Desk-Setup` Wi-Fi and enter your network (once).
2. Power-cycle the module: **3 minutes pairing window** (LED purple). Pair the phone (app: Control, Desk) and each robot ("Pair robot with desk").
3. App: Control, Desk: height, hold-to-move UP/DOWN, STOP, memory positions, child lock. Test, Checklist: Desk tests.
4. Chiku: long-press menu, "Desk control" (touch screen). Both robots react to the desk moving (look up/down, happy when it arrives).

## Status LED (the Super Mini's RGB LED)

purple = pairing window open · dim green = ready · blue = no Wi-Fi · amber = desk moving · red = locked out after bad messages.

## Height feedback

| Source | When | Accuracy |
|---|---|---|
| `display` | handset data line wired to GPIO11 and the frames are understood (Loctek / Jiecang style `F2 F2 cmd len data cs 7E`: height or 7-segment digits) | exact, live |
| `estimate` | not wired | from press time, calibrate with the number on the display (Control, Desk, Set); unknown after a memory-position travel until you calibrate |

The frame decoder follows a common protocol but **I could not verify it against your desk**: use *Raw capture* in the app to see what really arrives; send it to me if the height does not show.

## Safety

Dead-man (the desk stops within 0.5 s when messages stop), 25 s run limit, child lock, no UP+DOWN together, all outputs low at boot. Details in SECURITY.md.

## Code map (`src/`)

| File | Job |
|---|---|
| `buttons` | photorelay outputs, all safety rules, key sequences (save memory) |
| `height` | serial frame decoder, estimator, raw capture |
| `link` | DeskLink server: pairing, challenges, sealed commands, operations |
| `net` | Wi-Fi portal (first setup only), mDNS `desk.local`, OTA (pairing window only) |
| `storage`, `logbuf` | NVS (Wi-Fi, client keys), complete log (also readable with the `log` command) |

Library `Shared/DeskLink` (crypto + client for the robots), app code in `RobotApp/src/desk`.
