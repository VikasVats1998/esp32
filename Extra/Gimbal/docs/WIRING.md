# Gimbal module wiring (inside the Osmo Mobile SE)

> **Read this first.**
> * Opening the gimbal **voids the warranty** and can damage it. The Osmo has a **built-in lithium battery**: never short anything, switch the gimbal off, and use plastic tools.
> * **I have not seen the inside of your gimbal.** Everything below is a design for the usual cases plus checks you must do with a multimeter *before* soldering.
>   After opening it, **send me photos of the boards (both sides, joystick, buttons, zoom control)** and I will adapt the firmware and this guide to what is really there.
> * The module only needs to touch the user controls (joystick, buttons). Do not connect anything to the motor drivers, the battery or the phone connection.

## Before you solder (multimeter)

1. **Joystick type.** Find the joystick's pins (usually 4 to 6). With the gimbal **off**, measure resistance between the pins:
   * *Two potentiometers* (most common): about 10 k between two outer pins, and the wiper pin changes when you move the stick. **This is what the module supports.**
   * *Hall sensor / IC with its own output*: the output stays low impedance; do not inject into it with a plain resistor (tell me what you find).
   * *Digital 4-direction switches*: not supported yet (tell me).
2. **Wiper voltages** (gimbal **on**, black probe on gimbal GND): at the stick's centre each wiper should sit near half the supply (roughly 1.5 V on a 3 V rail). Note the **supply voltage of the joystick** (it must be at most 3.3 V) and the voltage at both extremes.
3. **Wiper impedance.** The wiper of a potentiometer is high impedance (a few kohm). Our injection uses a 470 ohm resistor so it overrides the pot and lets the pot take over again when we let go.
4. **Buttons:** shutter, M, trigger, and the zoom control. Find the two pads of each switch (continuity, gimbal off). Note if one pad is GND. The **zoom control** may be two switches (use ZOOM_ANALOG = 0) or a slide potentiometer (ZOOM_ANALOG = 1, a third PWM channel like the stick).
5. **A place for the module and its power.** The module draws about 80-120 mA while its radio listens. Measure the gimbal's 3.3 V rail first: only use it if it is a proper regulated rail with spare current. Otherwise use a **small separate LiPo (300 mAh) with its own switch**. Do not feed the battery voltage into the 3V3 pin.

## Parts

| Qty | Part | Note |
|---|---|---|
| 1 | ESP32-S3 Super Mini | |
| 6 | photorelay (TLP222A, AQY212GS, G3VM-61GR2) | shutter, M, trigger, zoom in, zoom out, spare. Polarity free |
| 6 | 330 ohm | photorelay LED resistor |
| 6 | 10 k ohm | GPIO to GND pull-down so nothing is pressed while the chip boots |
| 2 (3) | 470 ohm + 4.7 uF ceramic (per stick axis) | the RC filter: GPIO -> 470 ohm -> wiper node; 4.7 uF from the wiper node to GND |
| 2 (3) | 100 k ohm (optional) | only if you want the `learn` function: ADC pin to the wiper node |
| - | 30 AWG wire, heat shrink, kapton tape | |

## Pin table (Super Mini)

| GPIO | Function | Connects to |
|---|---|---|
| 1 | stick X out | 470 ohm -> joystick **X wiper node**; 4.7 uF from that node to GND |
| 2 | stick Y out | same for the **Y wiper node** |
| 4 | zoom out (only ZOOM_ANALOG = 1) | 470 ohm -> zoom slider wiper node |
| 12, 13, 14 | optional ADC inputs | 100 k -> X, Y (zoom) wiper node (read only, used by `learn`) |
| 5 | SHUTTER | photorelay 1 LED (330 ohm), 10 k to GND |
| 6 | M | photorelay 2 |
| 7 | TRIGGER | photorelay 3 |
| 8 | ZOOM IN | photorelay 4 (only ZOOM_ANALOG = 0) |
| 9 | ZOOM OUT | photorelay 5 |
| 10 | spare key | photorelay 6 |
| GND | | gimbal GND, photorelay LED cathodes |
| 3V3 / 5V | power | see "A place for the module and its power" |

Photorelay output side: **only** across the two pads of that button. All pins avoid GPIO 0, 3, 45, 46 and the USB pins; see `include/pins.h`.

### How the stick injection works

```
 gimbal MCU ADC pin ---+--- joystick wiper
                       |
                       +--- 4.7 uF --- GND
                       |
                       +--- 470 ohm --- ESP32 GPIO (PWM 78 kHz, 10 bit)
```
* While the remote is **idle** the GPIO is an input (Hi-Z): the physical stick works as before (the capacitor adds only about 10 ms of lag).
* While the remote **drives** the stick the PWM duty sets the voltage; the 470 ohm / 4.7 uF filter turns it into a clean DC level that overrides the pot.
* On **link loss** the module first holds the centre value for 150 ms (the gimbal stops turning) and then lets go.

## Calibration

The module needs the stick voltages for left / centre / right (per axis, in PWM counts 0..1023 = 0..3.3 V).

* **With the ADC wires:** `learn start`, leave the physical stick at rest for 1 s, move it to every extreme, `learn stop`. Stored in flash.
* **Without them:** measure the wiper voltage at the stick's centre and both ends with a multimeter and enter them: `cal set x <min> <center> <max>` (counts = volts / 3.3 x 1023). Check with `joy 127 0` (the stick output goes to the maximum for 0.8 s).

## Mounting

* Keep the antenna end of the Super Mini away from metal and the gimbal motors.
* Insulate the board; the gimbal is tight and flexes.
* Make sure the wires cannot touch the moving parts or get pinched by the housing.
* The ESP32's radio is on all the time: expect the gimbal's battery life to drop if you power the module from it.
