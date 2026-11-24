# Gimbal remote: test plan

## Bench (nothing connected to the gimbal yet)

1. Flash both. Gimbal console: `status`. Remote console: `status`.
2. **Pairing**: power-cycle the gimbal (LED purple), hold B1 + B4 on the remote for 3 s. Gimbal log: `PAIRED`, LED goes blue then green when the remote talks. `status` shows `link up` and frames counting.
3. **Stick outputs** (multimeter on the RC node of GPIO1 / GPIO2): `joy 127 0` -> voltage jumps to the calibrated maximum for 0.8 s, `joy -127 0` -> minimum, `joy 0 0` -> centre (default 1.65 V). Then Hi-Z again.
4. **Keys** (LED + 330 ohm on each photorelay, or multimeter across its output): `key shutter 500` etc. Keys must light one at a time.
5. **Remote -> gimbal**: move the remote stick: the RC node follows (smoothly, with dead zone and curve); hold B1: SHUTTER relay on; B2/B3: zoom relays.
6. **Macros**: `macro run 4` (recenter): TRIGGER relay twice. Remote B4 double tap: SWITCH_CAMERA sequence in the gimbal log.
7. **Failsafe**: with the stick deflected, switch the remote off: within 0.25 s the output goes to centre, then Hi-Z; keys release. Check `link` log line `FAILSAFE`.
8. **Replay**: not possible without tools; trust the PC tests. `status` counters `bad mac`, `replays`, `wrong session` should stay at 0 in normal use (wrong session may count 1-2 right after a gimbal restart).
9. **Range**: walk away; watch `rssi` in `status`. Typical open air 50 m or more; through your body less.

## In the gimbal (after soldering)

1. **Do not connect the photorelays yet.** Solder the stick injection first. With the gimbal on and the module idle (Hi-Z) the physical stick must behave exactly as before. If not, remove the capacitor or tell me.
2. `learn start` ... `learn stop` (or `cal set`), then `joy 60 0`: the gimbal should pan slowly. Check the direction (swap with JOY_INVERT_X/Y in `Extra/Remote/include/config.h`, or swap X/Y).
3. Connect the key relays one by one (`key trigger 150`, etc.) and watch what the gimbal / phone does.
4. Run each macro (`macro run N`) while watching the Mimo app and fix the sequences with `macro set`.
5. Failsafe test again with the gimbal on a table, camera off, first.

## PC tests

```bash
Extra/Shared/RemoteLink/test/run.sh      # frames, MAC, replay, pairing (19 checks)
Extra/Remote/test/run.sh                 # joystick mapping (12 checks)
```
