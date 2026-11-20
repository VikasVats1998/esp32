# Probe

Read-only measuring firmware for an ESP32-S3 Super Mini: finds out what unknown electronics do (desk handset, etc.)
without a scope. Serial console 115200: `help`, `pins`, `adc`, `watch <pin> [ms]`, `read`, `pull`, `edges <pin> [ms]`,
`baud <pin> [ms]`, `uart <pin> <baud> [ms]`. Guided session: `python3 Bringup/bringup.py probe desk`.

**Safety:** connect only through a 10 k resistor, only signals up to 3.3 V, GND first.

Build: `pio run -d Probe -e probe -t upload`
