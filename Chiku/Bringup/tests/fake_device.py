#!/usr/bin/env python3
"""Pretends to be a board on a serial port (a pty), so the toolkit can be tested without hardware.
   fake_device.py gimbal|remote|chiku|chiku-bad [--boot-delay S]   prints the port name and keeps running."""
import os
import pty
import sys
import threading
import time

PROFILE = sys.argv[1] if len(sys.argv) > 1 else "gimbal"
DELAY = float(sys.argv[sys.argv.index("--boot-delay") + 1]) if "--boot-delay" in sys.argv else 1.0

BOOT = {
    "gimbal": ["         5 I [storage] boot 3, paired", "        20 I [boot] gimbal module fw 1.0.0, reset reason 1", "        40 I [link] radio channel 1, my address AA:BB:CC:DD:EE:FF",
               "        41 I [link] pairing window open for 120 s: hold the pairing buttons on the remote"],
    "remote": ["         5 I [storage] boot 7, paired", "        30 I [boot] gimbal remote fw 1.0.0, reset reason 1", "        60 I [inputs] stick centre 2040 / 2051 (keep it at rest while switching on)"],
    "chiku": ["        10 I [boot] chiku fw 1.1.0, reset reason 1, heap 250 KB", "        11 I [diag] filesystem: ok LittleFS", "        12 I [diag] pmu: ok AXP2101", "        13 I [diag] psram: ok 8 MB expected",
              "        14 I [diag] rtc: ok PCF85063", "        15 I [diag] display: ok CO5300 466x466", "        16 I [diag] touch: ok CST9217", "        17 I [diag] imu: ok QMI8658",
              "        18 I [diag] audio: ok ES8311 + I2S", "        19 I [diag] pad_expander: ok PCF8574 0x20", "        20 I [diag] servo_driver: ok PCA9685 GPIO17/18"],
}
BOOT["gimbal-link"] = []
BOOT["remote-link"] = []
LINK_EVENTS = ["[link] stick frames", "[keys] shutter pressed", "[keys] zoomin pressed", "[keys] zoomout pressed", "[macro] run photo_video: m:80:0",
               "[macro] run switch_camera: m:80:150,m:80:0", "[macro] run recenter: trigger:80:150,trigger:80:0", "[link] FAILSAFE (no frames): keys released, sticks centred"]
BOOT["chiku-bad"] = [l.replace("pad_expander: ok PCF8574 0x20", "pad_expander: FAILED PCF8574 0x20").replace("I [diag] pad", "E [diag] pad") for l in BOOT["chiku"]] + ["       900 E [i2c] no ack from 0x20"]

STATUS = ["fw 1.0.0, session 3, paired, link down (failsafe), pairing window closed",
          "frames good 0, bad mac 0, replays 0, wrong session 0, rssi 0, send fails 0",
          "axis x: min 120 center 512 max 904", "axis y: min 120 center 512 max 904", "axis z: min 120 center 512 max 904",
          "keys pressed 0x00, injecting 0, macro running 0"]
MACROS = ["1 photo_video    m:80:0", "2 switch_camera  m:80:150,m:80:0", "3 active_track   (not configured)", "4 recenter       trigger:80:150,trigger:80:0",
          "5 flip           trigger:80:150,trigger:80:150,trigger:80:0", "6 lock           (not configured)", "7 sport          (not configured)"]


COUNTER = 0


def respond(cmd):
    c = cmd.strip()
    if PROFILE == "gimbal":
        if c == "status": return STATUS
        if c == "macro show": return MACROS
        if c == "cal show": return STATUS[2:5]
        if c == "wiper": return ["axis 0: 1650 mV", "axis 1: 1648 mV", "axis 2: -1 mV"]
        if c.startswith("joy"): return ["       100 I [console] joy 60 0 for 300 ms"]
        if c.startswith("key"): return [f"       100 I [console] {c.replace(' ', ' ', 1)} for 150 ms".replace("key ", "key ", 1).replace(" 150 for", " for")]
        if c == "macro run 4": return ["       100 I [macro] run recenter: trigger:80:150,trigger:80:0"]
        if c == "release": return []
    if PROFILE == "gimbal-link" and c == "status":
        global COUNTER
        COUNTER += 100
        return ["fw 1.0.0, session 3, paired, link up, pairing window closed", f"frames good {COUNTER}, bad mac 0, replays 0, wrong session 0, rssi -48, send fails 0"]
    if PROFILE == "remote" and c == "status":
        return ["fw 1.0.0, paired, link down, gimbal rssi 0, battery 3980 mV (79%), fine 0", "stick 0 1, keys 0x00, macro count 0", "axis 0 calibrated 340..3760", "axis 1 calibrated 355..3740"]
    return ["unknown command"]


def main():
    master, slave = pty.openpty()
    print(os.ttyname(slave), flush=True)
    boot = BOOT.get(PROFILE, [])

    def booter():
        time.sleep(DELAY)
        for l in boot:
            os.write(master, (l + "\r\n").encode())
            time.sleep(0.02)
    threading.Thread(target=booter, daemon=True).start()
    if PROFILE == "gimbal-link":
        def events():
            while COUNTER < 300:                       # start after the frame-counter check (third "status")
                time.sleep(0.1)
            time.sleep(1.0)
            for e in LINK_EVENTS:
                os.write(master, (f"      1000 I {e}\r\n").encode())
                time.sleep(1.2)
        threading.Thread(target=events, daemon=True).start()
    buf = b""
    while True:
        try:
            data = os.read(master, 256)
        except OSError:
            time.sleep(0.05)
            continue
        buf += data
        while b"\n" in buf:
            line, buf = buf.split(b"\n", 1)
            for out in respond(line.decode(errors="replace")):
                os.write(master, (out + "\r\n").encode())
                time.sleep(0.01)


main()
