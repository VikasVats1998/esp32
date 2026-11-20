# Chiku bring-up and test checklist

Use the phone app (`RobotApp`): **Test > Checklist** has a test for every screen, sensor, pad, motor and sound with pass/fail and a shareable report; **Console** shows the complete robot log and lets you send commands. Put the robot in **Manual** mode first. (Manual testing also works with the app's **Test** views or or the web page `http://chiku.local`.) API: `RobotApp/docs/API.md`.

## 1. Before power
- [ ] 5 V buck output measured (5.0 V) **before** connecting the PCA9685 V+; common GND everywhere.
- [ ] PCA9685 on GPIO17/18, PCF8574 (0x20) on the same two wires, TTP223 body pad on GPIO16.
- [ ] Servo horns **not** fitted yet.

## 2. Flash
- [ ] `pio run -e head -t upload` then `pio run -e head -t uploadfs`.
- [ ] ESP32-CAM: `pio run -e cam -t upload` (IO0 to GND, then remove and reset).

## 3. Self-test (hold BOOT while powering on)
Report on screen + serial. Expect PASS for: display, touch, imu, rtc, speaker, pca9685, camlink, sonar, pads, voice.
- Fails -> see the table at the end.

## 4. Test each part by hand (app: Test tab, mode = manual)
| Part | How | Expected |
|---|---|---|
| Display | Display test: red / green / blue / white / ramp | solid colours, smooth ramp, no dead lines |
| Touch | tap / swipe the screen, watch the event log | `tap`, `swipe` events |
| Faces | tap each of the 16 moods | matches the name, no flicker |
| Sound | beep 440 Hz, each melody, each voice clip, volume slider | clean, no crackle at 60 % |
| Servos | each slider 80 / 90 / 100 | correct servo, no buzzing, released after 2 s |
| Pads | touch each of the 5 pads once, twice, three times, hold | log shows pad + gesture; pad indicator lights |
| Sonar | hand at 10 / 30 / 100 cm | distance within ~2 cm |
| IMU | tilt / shake / lift | tilt value changes, `shake` / `pickup` events |
| Camera | wake, open stream, light on/off | live picture |
| Simulation | Sim buttons (pads, shake, fall) | the same reactions as the real thing |

## 5. Then calibrate
`docs/CALIBRATION.md`, then fit the horns, then try Move buttons and **Explore** mode on a table with a hand as obstacle.

## If a test fails
| Symptom | Check |
|---|---|
| `pca9685` FAIL | SDA/SCL swapped, no 3V3, address pads |
| `pads` FAIL / 0x00 | PCF8574 address (0x20), TTP223 VCC, wiring order P0-P3 |
| `sonar` FAIL | cam link first; ECHO on CAM GPIO2, TRIG GPIO15, 3.3 V sensor |
| `camlink` FAIL | TX/RX crossed? cam powered from 5 V? |
| `speaker` silent | PA pin, volume, mute switch in the app |
| servo jitter | separate 5 V supply, capacitor on V+ |
