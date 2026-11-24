# Using the gimbal remote

## Remote controls (defaults, `Extra/Remote/src/inputs.cpp`)

| Control | Action |
|---|---|
| Joystick | pan / tilt (the gimbal turns as if you moved its own stick) |
| B1 (hold) | capture: the gimbal's shutter is held while you hold B1 |
| B2 / B3 (hold) | zoom in / zoom out |
| B4 tap | photo / video switch (macro `photo_video`) |
| B4 double tap | switch front / rear camera (macro `switch_camera`) |
| B4 triple tap | lock (macro `lock`, **not configured by default**) |
| B4 long press | ActiveTrack (macro `active_track`, **not configured by default**) |
| Joystick click | recenter (macro `recenter`) |
| Joystick double click | flip / selfie (macro `flip`) |
| Joystick triple click | sport mode (macro `sport`, **not configured by default**) |
| Joystick long press | fine mode on / off (all stick movements 45 %) |
| B1 + B4 held 3 s | pairing |
| B2 + B3 held 3 s | joystick calibration (move the stick to every extreme for 6 s) |

LED: purple pairing · yellow calibrating · pink not paired · blue paired but no link · green linked (cyan-ish = fine mode) · white while a macro runs · red = the gimbal is in failsafe · orange blink = low battery.
Beeps (optional buzzer): rising = ok, low = error.

## Macros: the buttons you have to check

A macro is a short sequence of button presses the gimbal module performs. Defaults:

| Macro | Default sequence | Status |
|---|---|---|
| `recenter` | trigger, trigger (double press) | the usual DJI gesture, **verify** |
| `flip` | trigger x3 | the usual DJI gesture, **verify** |
| `photo_video` | M once | common, **verify** |
| `switch_camera` | M twice | common, **verify** |
| `active_track`, `lock`, `sport` | *empty* | I do not know your gimbal's gesture: **set them** |

Look up the real gestures in the Osmo Mobile SE manual or in the Mimo app (gimbal button settings) and set them over the USB console of the gimbal module:

```
macro show
macro set 4 trigger:80:150,trigger:80:0       # recenter = trigger twice (press 80 ms, pause 150 ms)
macro set 3 m:900:0                           # example: ActiveTrack = M held 0.9 s (CHECK your manual)
macro run 3                                   # try it without the remote
macro reset                                   # back to the defaults
```
Format: `key:press ms:pause ms`, comma separated, up to 6 steps; keys `shutter m trigger zoomin zoomout x`. Macros are stored in flash.

To change what the **remote** buttons trigger, edit the tables at the top of `Extra/Remote/src/inputs.cpp`.

## Things to know

* **Mimo app first.** The gimbal must be connected to the phone as usual (Bluetooth). The module only replaces your fingers.
* While the remote drives the stick, the physical stick is overridden; when you let go (idle 0.3 s) your own stick works again.
* If the remote switches off or goes out of range the gimbal stops turning within a quarter of a second.
* Remote battery: sleeps after 5 minutes idle; any button wakes it.
