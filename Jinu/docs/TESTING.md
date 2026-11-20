# Jinu bring-up and test checklist

Use the phone app (`RobotApp`): **Test > Checklist** has a test for every screen, sensor, pad, motor and sound with pass/fail and a shareable report; **Console** shows the complete robot log and lets you send commands. Put the robot in **Manual** mode first. (Manual testing also works with the app's **Test** views or or the web page `http://jinu.local`.) API: `RobotApp/docs/API.md`.

## 1. Before power
- [ ] Servo supply 5 V (>= 2 A) measured, common GND with the board. Horns **not** fitted yet.
- [ ] ECHO divider (1 k / 2 k) for a 5 V HC-SR04; MAX98357A VIN on 5 V.
- [ ] PCF8574 (0x20) on GPIO20/23, five TTP223 on P0..P4.

## 2. Flash
`pio run -e jinu -t upload` then `pio run -e jinu -t uploadfs` (hold BOOT + tap RESET if the port is missing).

## 3. Self-test (hold BOOT while powering on)
Expect PASS: display, servos (each moves), pads, touch (5/5), sonar, speaker, voice.

## 4. Test each part by hand (app: Test tab, mode = manual)
| Part | How | Expected |
|---|---|---|
| Display | colour test + grey ramp | solid colours, smooth ramp |
| Faces | all 16 moods | correct, smooth, RGB LED follows the colour |
| Sound | beep, melodies, clips, volume | clean sound, no hiss when idle |
| Servos | sliders 80 / 90 / 100 | correct servo, released after 2 s |
| Pads | 1 / 2 / 3 taps, long press on each pad | pad lights up, gesture in the event log |
| Sonar | hand at 10 / 30 / 100 cm | distance within ~2 cm |
| Simulation | Sim buttons | same reactions as the real touch |

## 5. Then calibrate and walk
`docs/CALIBRATION.md`, fit horns, Move buttons, then **Explore** mode.

## If a test fails
| Symptom | Check |
|---|---|
| `pads` FAIL | PCF8574 address 0x20 (not 0x38), SDA 20 / SCL 23 |
| no sound | amp SD pin (3V3 on many clones), I2S wires 4/5/17, mute in the app |
| sonar always -1 | ECHO divider, TRIG/ECHO swapped |
| board resets when servos move | servo supply must be separate, add capacitor |
