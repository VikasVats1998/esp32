# Desk module wiring (ESP32-S3 Super Mini inside the handset)

> **Read this first.**
> * Unplug the desk from the **mains** before you open the handset or touch the control box. The handset side is low voltage,
>   but the control box is not.
> * I could not see inside your handset. This guide is based on the photos (display `82.0`, keys **up, down, 1, 2, 3, 4, S** and one blank key)
>   and on how such handsets usually work. **Check each point in "Before you solder" with a multimeter first.** If something differs,
>   send me photos of the PCB and the cable plug and I will adapt the firmware.

## What the module does

* **Presses the buttons electrically**: each handset button is bridged by a photorelay (a tiny isolated switch) that the ESP32 switches on.
  The desk cannot tell it from a finger. Your own fingers still work (the handset keeps working).
* **Reads the height** (feedback): either from the handset cable's data line (read only) or, if that is not possible, it *estimates* the height from
  how long UP/DOWN were pressed (you calibrate it once with the number on the display).
* **Talks to Chiku, Jinu and the phone app** over Wi-Fi with an encrypted, authenticated link (see SECURITY.md).

## Parts

| Qty | Part | Note |
|---|---|---|
| 1 | ESP32-S3 Super Mini | the board you have |
| 8 | **Photorelay** (e.g. TLP222A, AQY212GS, G3VM-61GR2) | bidirectional, isolated, no polarity worries. 7 keys + 1 spare for the blank key |
| 8 | 330 ohm resistor | in series with each photorelay LED |
| 8 | 10 k ohm resistor | GPIO to GND, keeps every key released while the chip boots |
| - | thin flexible wire (30 AWG), heat shrink | |

Cheaper alternative to photorelays: PC817 optocouplers. They are **not** polarity free: the transistor's collector must go to the button pad that is at the
higher voltage when the key is not pressed (measure it). Photorelays are the safer choice.

## Before you solder (multimeter, desk unplugged and then powered via its handset cable only where noted)

1. **Find the two pads of every key.** Tactile switches have 2 (or 4, bridged in pairs) contacts. In continuity mode (desk unplugged) check that
   the key closes the two pads you chose.
2. **Is one pad GND?** (continuity to the handset's ground). Usual case: one pad is GND and the other goes to the handset's microcontroller.
   Photorelays do not care, but note it.
3. **Voltage on the pads** (handset powered, key not pressed): must be **3.3 V or 5 V**. If you measure more than 5 V, stop and tell me.
4. **Which wire carries the display data?** (optional, for height feedback) The handset is connected to the control box by a cable. Measure the idle level of
   each wire to GND: GND, a supply (5 V or more), and one or two data lines that sit near 3.3 V / 5 V and flicker. If a data line idles at **5 V**,
   use a divider (10 k + 20 k) before the ESP32 pin. A line above 5 V: do **not** connect it.
5. **Power for the module.** Measure the handset's supply rail. Safest for the first tests: power the Super Mini from a **separate USB cable** (thin cable through a hole in the
   handset case). Only later consider taking 5 V from the handset rail, and only if it is a true 5 V rail that can give 300 mA extra (Wi-Fi peaks). A weak rail can make the desk misbehave.

## Pin table

| Super Mini pin | Function | Connects to |
|---|---|---|
| GPIO1 | UP | photorelay 1 LED (via 330 ohm), 10 k to GND |
| GPIO2 | DOWN | photorelay 2 |
| GPIO4 | memory 1 | photorelay 3 |
| GPIO5 | memory 2 | photorelay 4 |
| GPIO6 | memory 3 | photorelay 5 |
| GPIO7 | memory 4 | photorelay 6 |
| GPIO8 | S | photorelay 7 |
| GPIO9 | X (blank key) | photorelay 8 |
| GPIO11 | height data in (optional) | handset cable data line controller -> handset (3.3 V only!) |
| GND | | common GND (handset GND, photorelay LED cathodes, data line GND) |
| 5V / USB-C | power | separate USB 5 V (recommended) |

Photorelay wiring (each): LED anode -> 330 ohm -> GPIO; LED cathode -> GND; output pins -> the two pads of the handset key.
**Never connect the photorelay output side to anything except the two pads of that key.**
All GPIOs avoid the strapping pins (0, 3, 45, 46) and the USB pins (19, 20). Pins are defined in `include/pins.h`.

## Mounting

* Keep the Super Mini's antenna end away from metal (the desk frame shields Wi-Fi). Check the signal in the app (Test > Desk).
* Insulate the board (heat shrink / tape) so nothing can short on the handset PCB.
* Strain-relieve every wire; the handset gets pressed thousands of times.

## First power-up and checks

1. Flash (`pio run -e desk -t upload`, hold BOOT while plugging in USB). The log shows `desk module fw ...` and a **device PIN**.
2. Wi-Fi: join the open network `Desk-Setup` and enter your Wi-Fi (first setup only).
3. Power-cycle the module: for 3 minutes it accepts pairing. Pair the **phone** (app: Control, Desk) and then each **robot** (same screen, "Pair robot with desk").
4. App: Test > Checklist runs all desk tests. Start with **key X, key S** (harmless), then **Nudge UP / DOWN**.
5. If the desk needs the memory key *held* while it travels, switch "Memory key must be held" on (Control, Desk, Memory positions).

## What can differ on your desk

| Possibility | What to do |
|---|---|
| a key does nothing | check the two pads you chose; try the other pair of contacts of the switch |
| the desk moves when the module boots | a 10 k pull-down is missing on that GPIO |
| memory keys need to be held | switch on "Memory key must be held" |
| stopping a memory travel does not work | any key press stops most desks; the module taps UP. If yours differs tell me which key stops it |
| the display line shows nothing | use Raw capture (Control, Desk, Diagnostics); wrong baud rate or protocol: send me the capture. Until then the estimate is used |
| desk goes only to a minimum/maximum | adjust `HEIGHT_MIN_CM` / `HEIGHT_MAX_CM` in `include/config.h` |
