# Jinu servo calibration

Do this **before** pressing the horns onto the servos.

1. Power the servo supply and the board. At boot all four servos go slowly to 90 degrees (soft start), then switch off after 2 s.
2. Open the web page `http://jinu.local` -> **Servo trims**. Press **Test** on a servo: it moves to 90 degrees *with* its trim.
3. Fit the horn so the part is in its centre position (legs straight down, feet flat). Fine tune with **trim** (degrees) and Save.
4. A servo that turns the wrong way: tick **inv**. Soft limits (min/max, default 10..170) protect the legs.
5. Check with Move -> walk / wiggle. For the hardware self-test hold **BOOT** while powering on: each servo moves +-15 degrees.

Gait tables use the Otto convention: the same sign on both ankles leans the body to one side. If a walk looks wrong, flip **inv** on one servo.
