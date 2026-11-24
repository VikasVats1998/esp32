#!/usr/bin/env python3
"""Bring-up toolkit for Chiku, Jinu, the desk module and the gimbal remote.

Run it on the laptop the boards are plugged into. It flashes, tests, asks you for the physical steps and writes a
report; paste Bringup/paste-me.txt into the chat when something fails. See README.md.
"""
import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

from lib import desktests, robothttp, serialtools  # noqa: E402
from lib.projects import PROJECTS, ROOT  # noqa: E402
from lib.report import FAIL, INFO, PASS, SKIP, Report  # noqa: E402

STATE_DIR = os.path.join(HERE, ".state")
ARGS = None


def ask(question):
    if ARGS and ARGS.yes:
        print(f"    ? {question}  -> yes (--yes)")
        return True
    while True:
        a = input(f"    ? {question} [y/n] ").strip().lower()
        if a in ("y", "yes", "j", "ja"):
            return True
        if a in ("n", "no", "nein"):
            return False


def pause(text):
    if ARGS and ARGS.yes:
        print(f"    > {text}  (--yes: not waiting)")
        return
    input(f"    > {text}  [press Enter when done] ")


def pick_port(preferred=None, what="the board"):
    if preferred:
        return preferred
    ports = serialtools.list_ports()
    esp = [p for p in ports if p["espressif"]] or ports
    if not esp:
        raise SystemExit("No serial port found. Plug the board in (data cable!) and try again. For the S3 Super Mini: hold BOOT while plugging in if it does not show up.")
    if len(esp) == 1:
        return esp[0]["device"]
    print(f"Several ports found, choose the one for {what}:")
    for i, p in enumerate(esp):
        print(f"  {i}: {p['device']}  {p['desc']}")
    return esp[int(input("number: "))]["device"]


# ------------------------------------------------------------------ commands
def cmd_ports(_):
    ports = serialtools.list_ports()
    if not ports:
        print("no serial ports found")
    for p in ports:
        tag = "  <- ESP32 (Espressif USB)" if p["espressif"] else ""
        print(f"{p['device']:<14} {p['desc']}{tag}")


def tool_version(args):
    try:
        out = subprocess.run(args, capture_output=True, text=True, timeout=30)
        return (out.stdout or out.stderr).strip().splitlines()[0]
    except Exception:
        return None


def cmd_doctor(_):
    print(f"python      {sys.version.split()[0]}")
    for mod in ("serial", "cryptography"):
        try:
            __import__(mod)
            print(f"{mod:<11} ok")
        except ImportError:
            print(f"{mod:<11} MISSING (run setup.sh / setup.ps1)")
    pio = tool_version([sys.executable, "-m", "platformio", "--version"])
    print(f"platformio  {pio or 'MISSING (run setup.sh / setup.ps1; needed only for flashing)'}")
    print(f"node        {tool_version(['node', '--version']) or 'not installed (only needed for the phone app)'}")
    print("repo root   " + ROOT)
    cmd_ports(None)


def cmd_flash(a):
    p = PROJECTS[a.project]
    rep = Report(HERE)
    print(f"Flashing {p['title']} ...")
    if p.get("manual_flash"):
        print("  ESP32-CAM: connect the USB-TTL adapter (5V, GND, TX->U0R, RX->U0T) and IO0 to GND, press RESET, then continue. Afterwards remove IO0-GND and press RESET.")
        pause("Ready?")
    port = a.port or (None if p.get("manual_flash") else pick_port(None, p["title"]))
    steps = [("firmware", "upload")] + ([("web page and voice clips", "uploadfs")] if p["fs"] else [])
    for name, target in steps:
        cmd = [sys.executable, "-m", "platformio", "run", "-d", os.path.join(ROOT, p["folder"]), "-e", p["env"], "-t", target]
        if port:
            cmd += ["--upload-port", port]
        out = subprocess.run(cmd, capture_output=True, text=True)
        text = (out.stdout + out.stderr).splitlines()
        rep.raw(f"flash-{a.project}-{target}", text)
        ok = out.returncode == 0
        rep.add("flash " + a.project, name, PASS if ok else FAIL, "" if ok else "see raw log", log=text[-15:])
    print("  (the board restarts by itself; if not, press its RESET button)")
    return finish(rep)


def boot_test(rep, project, port, seconds=10):
    p = PROJECTS[project]
    area = "boot " + project
    try:
        ser = serialtools.open_port(port, p["baud"])
    except Exception as e:
        rep.add(area, "open serial port", FAIL, str(e))
        return None
    serialtools.hard_reset(ser)
    lines = serialtools.read_lines(ser, seconds)
    ser.close()
    rep.raw(f"boot-{project}", lines)
    if not lines:
        rep.add(area, "serial output", FAIL, "nothing received: wrong port? board in boot loader (BOOT held)? try unplug/replug")
        return lines
    banner = p.get("banner")
    if banner:
        n = sum(1 for l in lines if re.search(banner, l))
        rep.add(area, "firmware starts", PASS if n == 1 else FAIL, "boot banner seen" if n == 1 else (f"banner seen {n} times: the board keeps rebooting" if n > 1 else "no boot banner (wrong firmware on this board?)"), log=lines[:12])
    for pat in p.get("expect", []):
        rep.add(area, f"log shows {pat}", PASS if any(re.search(pat, l) for l in lines) else FAIL)
    items = serialtools.diag_items(lines)
    for name in p.get("diag", []):
        if name in items:
            ok, detail = items[name]
            rep.add(area, f"hardware: {name}", PASS if ok else FAIL, detail, log=[l for l in lines if f"[diag] {name}" in l])
        else:
            rep.add(area, f"hardware: {name}", FAIL, "no result in the boot log")
    for desc, line in serialtools.find_problems(lines):
        if "[diag]" in line:
            continue                                    # already reported per item above
        rep.add(area, "problem in boot log", FAIL, desc, log=[line])
    return lines


def cmd_boot(a):
    rep = Report(HERE)
    boot_test(rep, a.project, pick_port(a.port, PROJECTS[a.project]["title"]))
    return finish(rep)


def cmd_gimbal(a):
    rep = Report(HERE)
    port = pick_port(a.port, "the gimbal module")
    lines = boot_test(rep, "gimbal", port, 6)
    ser = serialtools.open_port(port)
    area = "gimbal console"

    def run(cmd, wait=0.8):
        out = serialtools.command(ser, cmd, wait)
        rep.raw("gimbal-console", [f"> {cmd}"] + out)
        return out

    st = run("status")
    rep.add(area, "status", PASS if any("fw " in l for l in st) else FAIL, st[0] if st else "no answer", log=st)
    paired = any("paired" in l and "NOT" not in l for l in st)
    rep.add(area, "paired with a remote", PASS if paired else INFO, "" if paired else "not paired yet (see `link` test)")
    m = run("macro show")
    rep.add(area, "macros listed", PASS if sum(1 for l in m if re.match(r"\d ", l)) == 7 else FAIL, f"{len(m)} lines", log=m)
    rep.add(area, "recenter/flip/photo/camera macros configured", PASS if all(not re.search(rf"{n}\s+\(not configured\)", " ".join(m)) for n in ("recenter", "flip", "photo_video", "switch_camera")) else FAIL, "")
    unset = [n for n in ("active_track", "lock", "sport") if re.search(rf"{n}\s+\(not configured\)", " ".join(m))]
    rep.add(area, "ActiveTrack / lock / sport macros", INFO if unset else PASS, ("NOT configured: " + ", ".join(unset) + " (set them with `macro set`, see Gimbal/docs/USAGE.md)") if unset else "configured")
    c = run("cal show")
    rep.add(area, "calibration present", PASS if sum(1 for l in c if l.startswith("axis")) >= 3 else FAIL, log=c)
    w = run("wiper")
    mv = [int(x) for l in w for x in re.findall(r"axis \d: (-?\d+) mV", l)]
    rep.add(area, "joystick wiper voltages (optional ADC wires)", INFO, "mV " + ", ".join(map(str, mv)) if mv else "no answer", log=w)
    if a.outputs:
        print("  --outputs: the module now drives its outputs. Only do this when nothing is connected to the gimbal, or you are ready for the gimbal to react.")
        if ask("Continue?"):
            for cmd, expect in (("joy 60 0 300", r"\[console\] joy 60 0"), ("key x 150", r"\[console\] key x"), ("key shutter 150", r"\[console\] key shutter"), ("macro run 4", r"\[macro\] run recenter")):
                out = run(cmd, 1.2)
                rep.add(area, cmd, PASS if any(re.search(expect, l) for l in out) else FAIL, log=out)
                time.sleep(0.5)
            run("release")
    else:
        rep.add(area, "output commands", SKIP, "use --outputs to test joy/key/macro (drives the relays/PWM!)")
    ser.close()
    return finish(rep)


def cmd_remote(a):
    rep = Report(HERE)
    port = pick_port(a.port, "the remote")
    boot_test(rep, "remote", port, 6)
    ser = serialtools.open_port(port)
    out = serialtools.command(ser, "status", 0.8)
    rep.raw("remote-console", out)
    rep.add("remote console", "status", PASS if any("fw " in l for l in out) else FAIL, out[0] if out else "no answer", log=out)
    for l in out:
        m = re.search(r"battery (\d+) mV", l)
        if m:
            mv = int(m.group(1))
            rep.add("remote console", "battery reading", PASS if 3000 < mv < 4400 else FAIL, f"{mv} mV (a LiPo is 3.3-4.2 V; ~0 means no divider wired)")
    print("  Stick at rest: reading values 5 s, then move the stick and press the buttons ...")
    pause("Leave the stick at rest")
    rest = serialtools.command(ser, "status", 0.8)
    jx = [re.search(r"stick (-?\d+) (-?\d+)", l) for l in rest]
    jx = [m for m in jx if m]
    if jx:
        x, y = int(jx[0].group(1)), int(jx[0].group(2))
        rep.add("remote console", "stick centred at rest", PASS if abs(x) <= 2 and abs(y) <= 2 else FAIL, f"x {x}, y {y}")
    ser.close()
    return finish(rep)


def wait_for(ser, pattern, seconds, lines_sink):
    got = serialtools.read_lines(ser, seconds, stop_when=lambda l: re.search(pattern, l))
    lines_sink.extend(got)
    return any(re.search(pattern, l) for l in got), got


def cmd_link(a):
    rep = Report(HERE)
    print("Both boards must be plugged into this computer.")
    gport = pick_port(a.gimbal_port, "the GIMBAL module")
    rport = pick_port(a.remote_port, "the REMOTE")
    g, r = serialtools.open_port(gport), serialtools.open_port(rport)
    glog, rlog = [], []
    area = "gimbal link"
    st = serialtools.command(g, "status", 0.8)
    if any("NOT paired" in l for l in st) or a.pair:
        print("  Pairing: power-cycle the gimbal module (unplug/replug it), then hold B1 + B4 on the remote for 3 s.")
        pause("Do it now, then press Enter and wait")
        ok_g, _ = wait_for(g, r"PAIRED", 40, glog)
        ok_r, _ = wait_for(r, r"PAIRED", 10, rlog)
        rep.add(area, "pairing", PASS if ok_g and ok_r else FAIL, f"gimbal {'ok' if ok_g else 'no'}, remote {'ok' if ok_r else 'no'}", log=glog[-8:] + rlog[-8:])
    st = serialtools.command(g, "status", 1.0)
    rep.raw("link-gimbal", st)
    rep.add(area, "link up", PASS if any("link up" in l for l in st) else FAIL, st[0] if st else "", log=st)

    def good_frames():
        out = serialtools.command(g, "status", 0.8)
        for l in out:
            m = re.search(r"frames good (\d+)", l)
            if m:
                return int(m.group(1))
        return None

    print("  Move the remote stick around for 4 seconds ...")
    before = good_frames()
    time.sleep(4)
    after = good_frames()
    rep.add(area, "control frames arrive", PASS if before is not None and after is not None and after - before > 20 else FAIL,
            f"frames good {before} -> {after}" + ("" if before is not None else " (no answer to status)"))
    steps = [
        ("HOLD button B1 (capture) for 1 s", g, r"shutter pressed", "shutter key pressed on the gimbal side"),
        ("HOLD button B2 (zoom in) for 1 s", g, r"zoomin pressed", "zoom in key pressed"),
        ("HOLD button B3 (zoom out) for 1 s", g, r"zoomout pressed", "zoom out key pressed"),
        ("TAP button B4 once", g, r"\[macro\] run photo_video", "macro photo_video"),
        ("DOUBLE-TAP button B4", g, r"\[macro\] run switch_camera", "macro switch_camera"),
        ("CLICK the joystick once", g, r"\[macro\] run recenter", "macro recenter"),
    ]
    for prompt, ser, pat, name in steps:
        print(f"  {prompt}  (10 s)")
        ok, got = wait_for(ser, pat, 10, glog)
        rep.add(area, name, PASS if ok else FAIL, "" if ok else "not seen in 10 s", log=got[-8:])
    print("  FAILSAFE test: switch the remote OFF (unplug its battery/USB) now (20 s)")
    ok, got = wait_for(g, r"FAILSAFE", 20, glog)
    rep.add(area, "failsafe when the remote disappears", PASS if ok else FAIL, "keys released, sticks centred" if ok else "no FAILSAFE log within 20 s", log=got[-6:])
    rep.raw("link-gimbal-log", glog)
    g.close()
    r.close()
    return finish(rep)


def cmd_robot(a):
    rep = Report(HERE)
    robothttp.run(rep, a.host, ask, active=a.active, kind=a.kind, simulate=ARGS.yes)
    return finish(rep)


def cmd_desk(a):
    rep = Report(HERE)
    desktests.run(rep, a.host, STATE_DIR, ask, pair=a.pair, active=a.active)
    return finish(rep)


def cmd_probe(a):
    """Guided measurement session with the Probe firmware: finds out what the gimbal / desk electronics really do."""
    rep = Report(HERE)
    port = pick_port(a.port, "the PROBE board")
    ser = serialtools.open_port(port)
    serialtools.hard_reset(ser)
    time.sleep(1.5)
    ser.reset_input_buffer()

    def run(cmd, wait=0.5):
        out = serialtools.command(ser, cmd, wait)
        rep.raw("probe", [f"> {cmd}"] + out)
        return out

    def grab(out, key, regex):
        for l in out:
            if l.startswith(key):
                m = re.search(regex, l)
                if m:
                    return int(float(m.group(1)))
        return None

    print("SAFETY: probe wires only through a 10 k resistor, only signals up to 3.3 V, GND first. Never touch mains or a battery directly.")
    area = "probe " + a.session
    run("help")
    if a.session == "gimbal":
        axes = [("X", 1, "joystick X wiper"), ("Y", 2, "joystick Y wiper")]
        cal = {}
        for name, pin, desc in axes:
            print(f"\n  Connect GPIO{pin} (through 10k) to the {desc} on the gimbal board, GND to the gimbal GND. Gimbal ON.")
            pause("Connected?")
            res = {}
            for label, text in (("centre", "leave the stick at REST"), ("min", "hold the stick fully to one side (A)"), ("max", "hold the stick fully to the OPPOSITE side (B)")):
                pause(f"{text}, then Enter (measures 2 s)")
                out = run(f"watch {pin} 2000", 2.5)
                res[label] = grab(out, "watch", r"avg=(\d+)")
            if None in res.values():
                rep.add(area, f"axis {name}", FAIL, "no readings (is the probe board running the Probe firmware?)")
                continue
            lo, hi = sorted([res["min"], res["max"]])
            cal[name] = (round(lo * 1023 / 3300), round(res["centre"] * 1023 / 3300), round(hi * 1023 / 3300))
            plausible = hi - lo > 600 and lo < res["centre"] < hi
            rep.add(area, f"axis {name} range", INFO if plausible else FAIL, f"min {lo} mV, centre {res['centre']} mV, max {hi} mV" + ("" if plausible else " (range looks too small or not a potentiometer wiper)"))
            if plausible:
                rep.note(f"gimbal console: cal set {name.lower()} {cal[name][0]} {cal[name][1]} {cal[name][2]}")
        print("\n  Buttons: probe the pad of a button that goes to the gimbal's processor (through 10k). Test each: shutter, M, trigger.")
        for nm, pin in (("shutter", 5), ("M", 6), ("trigger", 7)):
            if not ask(f"Test the {nm} button now (probe on GPIO{pin})?"):
                continue
            pause(f"Do NOT press {nm}")
            idle = grab(run(f"watch {pin} 1200", 1.7), "watch", r"avg=(\d+)")
            pause(f"HOLD {nm} pressed")
            pressed = grab(run(f"watch {pin} 1200", 1.7), "watch", r"avg=(\d+)")
            rep.add(area, f"{nm} contact", INFO, f"idle {idle} mV, pressed {pressed} mV")
    else:  # desk
        print("\n  Connect GPIO11 (through 10k) to the handset cable data line (controller -> handset), GND to desk GND. Desk ON.")
        pause("Connected?")
        pause("Move the desk a little (or press UP on the handset) so the line is active")
        out = run("edges 11 1500", 2.0)
        rep.add(area, "activity on the line", INFO, out[0] if out else "no answer", log=out)
        out = run("baud 11 3000", 3.5)
        rep.add(area, "baud rate guess", INFO, out[0] if out else "no answer", log=out)
        guess = grab(out, "baud", r"guess=(\d+)")
        best = None
        for baud in ([guess] if guess else []) + [9600, 19200, 115200, 38400, 57600, 4800]:
            out = run(f"uart 11 {baud} 2500", 3.0)
            n = grab(out, "uart", r"bytes=(\d+)") or 0
            hexl = next((l for l in out if l.startswith("hex ")), "")
            looks = "F2 F2" in hexl or "F1 F1" in hexl
            rep.add(area, f"serial dump at {baud} baud", INFO, f"{n} bytes" + (", contains F2 F2 / F1 F1 frame headers" if looks else ""), log=out)
            if looks:
                best = baud
                break
        if best:
            rep.note(f"desk module: set DESK_BAUD {best} in Desk/include/config.h")
    ser.close()
    return finish(rep)


def identify(port):
    """Which firmware runs on this port? Reads the boot log."""
    try:
        ser = serialtools.open_port(port)
    except Exception:
        return None
    serialtools.hard_reset(ser)
    lines = serialtools.read_lines(ser, 4)
    ser.close()
    for name, p in PROJECTS.items():
        if p.get("banner") and any(re.search(p["banner"], l) for l in lines):
            return name
    return "unknown" if lines else "silent"


def cmd_all(a):
    rep = Report(HERE)
    ports = [p for p in serialtools.list_ports() if p["espressif"]]
    if not ports:
        rep.add("all", "boards found", FAIL, "no ESP32 USB ports found")
        return finish(rep)
    for p in ports:
        proj = identify(p["device"])
        rep.add("all", f"{p['device']}", INFO, f"runs: {proj}")
        if proj in PROJECTS:
            boot_test(rep, proj, p["device"], 8)
    print("\nFor Chiku / Jinu / Desk also run: bringup.py robot <host> --active | bringup.py desk <host>   (network tests)")
    return finish(rep)


def finish(rep):
    md, paste = rep.write()
    c = rep.counts()
    print(f"\n{c[PASS]} passed, {c[FAIL]} failed, {c[SKIP]} skipped")
    print(f"report:  {md}")
    print(f"paste this into the chat if anything failed:  {os.path.join(HERE, 'paste-me.txt')}")
    return 1 if c[FAIL] else 0


def main():
    global ARGS
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--yes", action="store_true", help="answer every question with yes and do not wait (for dry runs against simulators)")
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("ports", help="list serial ports")
    sub.add_parser("doctor", help="check the tools on this computer")
    p = sub.add_parser("flash", help="flash a project"); p.add_argument("project", choices=PROJECTS); p.add_argument("--port")
    p = sub.add_parser("boot", help="reset a board and check its boot log"); p.add_argument("project", choices=PROJECTS); p.add_argument("--port")
    p = sub.add_parser("gimbal", help="test the gimbal module over USB serial"); p.add_argument("--port"); p.add_argument("--outputs", action="store_true", help="also drive the outputs")
    p = sub.add_parser("remote", help="test the handheld remote over USB serial"); p.add_argument("--port")
    p = sub.add_parser("link", help="guided radio link test (gimbal + remote both on USB)"); p.add_argument("--gimbal-port"); p.add_argument("--remote-port"); p.add_argument("--pair", action="store_true")
    p = sub.add_parser("robot", help="test Chiku / Jinu over Wi-Fi"); p.add_argument("host"); p.add_argument("--kind"); p.add_argument("--active", action="store_true", help="moves servos, makes sound (robot on a stand!)")
    p = sub.add_parser("desk", help="test the desk module over Wi-Fi (secure link)"); p.add_argument("host"); p.add_argument("--pair", action="store_true"); p.add_argument("--active", action="store_true", help="the desk WILL move a little")
    p = sub.add_parser("probe", help="guided measurements with the Probe board"); p.add_argument("session", choices=["gimbal", "desk"]); p.add_argument("--port")
    sub.add_parser("all", help="find every ESP32 board on USB and check its boot")
    ARGS = ap.parse_args()
    handler = {"ports": cmd_ports, "doctor": cmd_doctor, "flash": cmd_flash, "boot": cmd_boot, "gimbal": cmd_gimbal, "remote": cmd_remote,
               "link": cmd_link, "robot": cmd_robot, "desk": cmd_desk, "probe": cmd_probe, "all": cmd_all}[ARGS.cmd]
    sys.exit(handler(ARGS) or 0)


if __name__ == "__main__":
    main()
