#!/usr/bin/env python3
"""Live terminal UI for the joystick remote test.
Usage: python3 remote_monitor.py [port]      (default: auto-detect /dev/ttyACM* or /dev/ttyUSB*)
Keys:  r = recalibrate centre / reset min-max     q = quit
Needs: pip install pyserial   (and your user in the 'dialout' group)
"""
import curses, glob, sys, time
import serial

def find_port():
    ports = sorted(glob.glob("/dev/ttyACM*") + glob.glob("/dev/ttyUSB*"))
    return ports[0] if ports else None

def pct(raw, c):
    v = (raw - c) * 100 / max(1, (4095 - c) if raw >= c else c)
    return max(-100, min(100, v))

def verdict(c, mn, mx, rate_ok):
    if not rate_ok: return ("NO DATA", 1)
    if c < 200 or c > 3900: return ("BAD: centre at rail - check VCC/wiring", 1)
    if mx - mn < 300: return ("waiting - move the stick", 3)
    if mn <= 150 and mx >= 3950: return ("OK full range", 2)
    return ("OK so far - push to all edges", 3)

def main(scr, port):
    curses.curs_set(0); scr.nodelay(True)
    curses.start_color(); curses.use_default_colors()
    curses.init_pair(1, curses.COLOR_RED, -1)
    curses.init_pair(2, curses.COLOR_GREEN, -1)
    curses.init_pair(3, curses.COLOR_YELLOW, -1)
    curses.init_pair(4, curses.COLOR_CYAN, -1)

    ser = serial.Serial(port, 115200, timeout=0.05)
    cx = cy = None
    mn = [4095, 4095]; mx = [0, 0]
    raw = [2048, 2048]; sw = btn = 0
    swn = btnn = 0; swp = btp = 0
    swok = btok = False
    count = 0; t0 = time.time(); last = 0; hz = 0; trail = []
    calib = []

    def reset():
        nonlocal cx, cy, calib
        cx = cy = None; calib = []
        mn[:] = [4095, 4095]; mx[:] = [0, 0]

    while True:
        k = scr.getch()
        if k in (ord('q'), 27): return
        if k == ord('r'): reset()
        for _ in range(40):
            line = ser.readline().decode(errors="ignore").strip()
            if not line: break
            p = line.split(",")
            if len(p) != 5 or p[0] != "D": continue
            try: raw = [int(p[1]), int(p[2])]; sw = int(p[3]); btn = int(p[4])
            except ValueError: continue
            count += 1; last = time.time()
            if cx is None:                      # average first 25 samples as centre
                calib.append(raw[:])
                if len(calib) >= 25:
                    cx = sum(c[0] for c in calib) // 25; cy = sum(c[1] for c in calib) // 25
                    mn[:] = [cx, cy]; mx[:] = [cx, cy]
                continue
            for i in range(2):
                mn[i] = min(mn[i], raw[i]); mx[i] = max(mx[i], raw[i])
            if sw and not swp: swn += 1; swok = True
            if btn and not btp: btnn += 1; btok = True
            swp, btp = sw, btn
        if time.time() - t0 >= 1: hz = count; count = 0; t0 = time.time()
        alive = time.time() - last < 0.5

        scr.erase()
        h, w = scr.getmaxyx()
        scr.addstr(0, 0, f" REMOTE TEST  {port}  {hz} Hz   [r] recalibrate  [q] quit ", curses.A_REVERSE)
        if not alive:
            scr.addstr(2, 2, "NO DATA from board - check USB / flash RemoteTest.ino", curses.color_pair(1) | curses.A_BOLD)
            scr.refresh(); time.sleep(0.05); continue
        if cx is None:
            scr.addstr(2, 2, "Calibrating centre... release the stick", curses.color_pair(3))
            scr.refresh(); continue

        px, py = pct(raw[0], cx), pct(raw[1], cy)
        # joystick pad (25x13 box)
        bw, bh = 41, 15
        ox, oy = 3, 2
        for r in range(bh):
            scr.addstr(oy + r, ox, "│" + " " * (bw - 2) + "│")
        scr.addstr(oy, ox, "┌" + "─" * (bw - 2) + "┐")
        scr.addstr(oy + bh - 1, ox, "└" + "─" * (bw - 2) + "┘")
        mxp, myp = ox + bw // 2, oy + bh // 2
        for r in range(1, bh - 1): scr.addstr(oy + r, mxp, "┆", curses.color_pair(4))
        scr.addstr(myp, ox + 1, "┄" * (bw - 2), curses.color_pair(4))
        dx = mxp + int(px / 100 * (bw // 2 - 2)); dy = myp - int(py / 100 * (bh // 2 - 1))
        trail.append((dx, dy)); trail[:] = trail[-12:]
        for tx, ty in trail[:-1]: scr.addstr(ty, tx, "·", curses.color_pair(3))
        scr.addstr(dy, dx, "◉" if sw else "●", curses.color_pair(1 if sw else 2) | curses.A_BOLD)
        scr.addstr(oy - 1, mxp - 1, "UP"); scr.addstr(oy + bh, mxp - 2, "DOWN")

        # right panel
        rx = ox + bw + 3
        def bar(v, wd=30):
            pos = int((v + 100) * (wd - 1) / 200)
            return "".join("█" if i == pos else ("│" if i == wd // 2 else "─") for i in range(wd))
        scr.addstr(oy,     rx, f"X (GPIO0) raw {raw[0]:4d}  {px:+4.0f}%")
        scr.addstr(oy + 1, rx, bar(px), curses.color_pair(2))
        scr.addstr(oy + 3, rx, f"Y (GPIO1) raw {raw[1]:4d}  {py:+4.0f}%")
        scr.addstr(oy + 4, rx, bar(py), curses.color_pair(2))
        scr.addstr(oy + 6, rx, f"Joy click  GPIO3 : {'■ PRESSED' if sw else '□ released'}  x{swn}",
                   curses.color_pair(1 if sw else 0) | (curses.A_BOLD if sw else 0))
        scr.addstr(oy + 7, rx, f"Button    GPIO10 : {'■ PRESSED' if btn else '□ released'}  x{btnn}",
                   curses.color_pair(1 if btn else 0) | (curses.A_BOLD if btn else 0))
        scr.addstr(oy + 9, rx, "CHECKLIST", curses.A_BOLD)
        for i, (name, c, a, b) in enumerate([("X axis", cx, mn[0], mx[0]), ("Y axis", cy, mn[1], mx[1])]):
            t, col = verdict(c, a, b, alive)
            scr.addstr(oy + 10 + i, rx, f"{name}: {t}  [{a}..{b}] c={c}", curses.color_pair(col))
        scr.addstr(oy + 12, rx, f"Joy click: {'OK' if swok else 'press it'}", curses.color_pair(2 if swok else 3))
        scr.addstr(oy + 13, rx, f"Button   : {'OK' if btok else 'press it'}", curses.color_pair(2 if btok else 3))
        allok = swok and btok and mn[0] <= 150 and mx[0] >= 3950 and mn[1] <= 150 and mx[1] >= 3950
        if allok:
            scr.addstr(oy + 15, rx, "ALL GOOD - remote hardware works!", curses.color_pair(2) | curses.A_BOLD)
        scr.refresh(); time.sleep(0.02)

if __name__ == "__main__":
    port = sys.argv[1] if len(sys.argv) > 1 else find_port()
    if not port: sys.exit("No serial port found. Plug in the board (or pass the port as an argument).")
    try:
        curses.wrapper(main, port)
    except serial.SerialException as e:
        sys.exit(f"Cannot open {port}: {e}\nTip: sudo usermod -aG dialout $USER (then re-login), and close the Arduino Serial Monitor.")
    except KeyboardInterrupt:
        pass
