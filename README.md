# ESP32 robots and accessories

| Folder | What |
|---|---|
| [Chiku/](Chiku/README.md) | **Chiku**, the main robot, and everything it needs: firmware (head + ESP32-CAM), hub server, phone app, test toolkit, standing-desk accessory, servo test, shared DeskLink library, flash backups |
| [Extra/](Extra/README.md) | all other projects: Jinu, OTTO, universal remote joystick, gimbal remote (paused), Probe, old EMO sketches, reference projects |

Install once: `pip install platformio "click<8.2"` (click 8.2+ breaks the bundled esptool).
All commands in the docs are run from this top folder, for example `pio run -d Chiku/Firmware -e head`.
