# Calibration: screen orientation and servos

Both also run in the **first-time setup** (steps 3 and 4). Later: swipe up on the face for the menu,
then **Settings**, then **Screen orientation** or **Servo calibration**.

## Screen orientation (any angle)

The round panel can sit in the head at any angle, not only 0 / 90 / 180 / 270. Chiku turns the picture
and the touch input by any number of degrees (0-359).

1. Stand Chiku upright on the table, screen facing you.
2. Open **Screen orientation**. The arrow should point straight up and the cyan line should be level
   with the table.
3. Tap **Auto**: the IMU measures where "down" is in the panel plane and sets the exact angle.
   The line under the buttons says how far off the current angle is ("Level OK" when it fits).
4. Fine-tune with **- / +** (1 degree, hold for 5-degree steps) until the arrow points straight up.
5. **Save** (stored in NVS). Leaving without saving goes back to the old angle.

The angle can also be set from the web page (Screen, Rotation) or `POST /api/rotation?deg=27&save=1`.

**Check once (assumption):** the mapping of the IMU axes to the panel is guessed in `include/config.h`
(`IMU_SCREEN_X`, `IMU_SCREEN_Y`). If **Auto** turns the picture the wrong way (for example it gives
-30 degrees when the panel needs +30 degrees), flip the sign of `IMU_SCREEN_X` and flash again.

## Servos (before mounting the horns)

Do this **before** pressing the servo horns on the shafts.

1. Power the body (5 V buck) with the PCA9685 connected to the head.
2. Boot the head. The servos soft-start to 90 degrees one after another.
3. Open **Servo calibration**. The body diagram shows the 5 joints: neck, left hip, right hip,
   left foot, right foot. The yellow joint is the one being set; it moves to its centre (90 + trim).
4. Mount the horn so the part points to its centre: hips with the legs straight down, feet flat,
   neck looking forward.
5. Fine-tune with **- / +** (1 degree; the label shows the angle, for example "L FOOT 88").
   If a servo turns the wrong way, tap **reverse direction**.
6. **Save** stores that joint (green tick) and goes to the next one. After the right foot everything
   is saved in NVS.
7. Soft limits (min/max, default 10..170) can be changed on the web page (http://chiku.local, "Servo trims").
8. Check with Menu, Moves (walk, wiggle). The hardware self-test (hold BOOT at power-on) moves every servo slowly.

Tips: screw the horn only after the servo is at 90. Never force a servo by hand when it is powered.
