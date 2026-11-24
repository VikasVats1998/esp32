# Bringup: test the real boards from your laptop

Runs on the computer the boards are plugged into (this repo's cloud workspace cannot see USB devices).
It flashes, runs the tests, asks you for the physical steps (touch this, did you hear that) and writes a report.
**When something fails, paste `Bringup/paste-me.txt` into the chat** instead of describing it.

## Setup

```bash
pip install platformio "click<8.2" pyserial cryptography
python3 Bringup/bringup.py doctor        # checks the tools and lists serial ports
```

## Commands

| Command | Does |
|---|---|
| `bringup.py ports` | list serial ports (ESP32 boards are marked) |
| `bringup.py flash <chiku\|chiku-cam\|jinu\|desk\|probe>` | flash firmware (and web page / voice clips) |
| `bringup.py boot <project>` | reset the board, check its boot log: hardware self-check items, crashes, reboot loops |
| `bringup.py all` | find every ESP32 on USB and check its boot |
| `bringup.py robot <host> [--active]` | Chiku / Jinu over Wi-Fi (Chiku also: microphones, screen rotation). `--active` also shows every face (and on Chiku every screen), moves servos, plays sound, asks you to touch each pad and to clap for the mics (robot on a stand) |
| `bringup.py desk <host> [--pair] [--active]` | desk module over the secure link. `--active` moves the desk a little |
| `bringup.py probe desk` | guided measurement of the desk handset data line with the Probe board |

Add `--yes` only for dry runs against simulators (answers every question with yes).
Results: `Bringup/reports/<time>/report.md` (full) and `Bringup/paste-me.txt` (short).
The desk key from `--pair` is saved in `Bringup/.state/desk.json` (not in git, keep it private).

## Testing the toolkit itself

`Bringup/tests/selftest.sh` runs it against the RobotApp mocks and simulated serial boards.
Known issue: the gimbal `link` simulation step fails (gimbal project paused).
