# Servo calibration (before mounting the horns)

Do this **before** pressing the servo horns on the shafts.

1. Power the body (5 V buck) with the PCA9685 connected to the head.
2. Boot the head. Servos are soft-started to 90 degrees one after another.
   Leave them powered for a few seconds, then the outputs switch off again (2 s idle).
3. Long-press the screen -> **Menu** -> **Calibrate servos** -> OK.
   The screen shows the servo name (0 left hip ... 4 head pan) and moves it to 90 degrees.
4. Mount the horn so the part points to its centre position:
   * hips: legs straight down, ankles: feet flat, head: looking forward.
5. Fine tune with **- / +** (trim in degrees). The servo moves at once.
   If a servo turns the wrong way, press **Invert**. (Right side servos are inverted by default.)
6. Select the next servo with **< / >**. Press **Save** at the end (stored in NVS).
7. Soft limits (min/max, default 10..170) can be changed on the web page (http://chiku.local, "Servo trims").
8. Check with Menu -> Walk / Wiggle. Run the hardware self-test (hold BOOT at power-on) to move every servo slowly.

Tips: screw the horn only after the servo is at 90. Never force a servo by hand when it is powered.
