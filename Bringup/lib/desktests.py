"""Tests of the desk module over its secure link."""
import json
import os
import time

from .desklink_py import Desk, DeskError
from .report import FAIL, INFO, PASS, SKIP


def run(report, host, state_dir, ask, pair=False, active=False):
    area = f"desk {host}"
    path = os.path.join(state_dir, "desk.json")
    desk = Desk(host)
    try:
        info = desk.info()
    except DeskError as e:
        report.add(area, "reachable", FAIL, str(e))
        return
    report.add(area, "reachable", PASS, f"fw {info['fw']}, {info['clients']} client(s), pairing {'OPEN ' + str(info['left']) + ' s' if info['pairing'] else 'closed'}")
    if os.path.exists(path) and not pair:
        saved = json.load(open(path))
        if saved.get("host") == host:
            desk.creds = saved["creds"]
    if pair or not desk.creds:
        if not info["pairing"]:
            report.add(area, "pairing", SKIP, "pairing window closed: power-cycle the desk, then run again with --pair")
            if not desk.creds:
                return
        else:
            try:
                creds = desk.pair("bringup laptop")
                os.makedirs(state_dir, exist_ok=True)
                json.dump({"host": host, "creds": creds}, open(path, "w"))
                os.chmod(path, 0o600)
                report.add(area, "pairing", PASS, f"client id {creds['id']} (key saved in Bringup/.state/desk.json, keep it private)")
            except DeskError as e:
                report.add(area, "pairing", FAIL, str(e))
                return
    try:
        t0 = time.time()
        s = desk.call({"op": "state"})
        report.add(area, "secure link", PASS if s.get("ok") else FAIL, f"sealed round trip {int((time.time() - t0) * 1000)} ms")
    except DeskError as e:
        report.add(area, "secure link", FAIL, str(e))
        return
    report.add(area, "height known", PASS if s.get("hv") else FAIL, f"{s.get('h')} cm from the {s.get('src')}" if s.get("hv") else "unknown: calibrate with the app or: setheight")
    report.add(area, "display line decoded (optional)", PASS if s.get("frames", 0) > 0 else INFO,
               f"{s.get('frames')} frames ok / {s.get('bad')} bad" if s.get("frames", 0) else f"{s.get('bytes', 0)} bytes seen, no valid frame (estimate used)")
    report.add(area, "no key pressed at rest", PASS if "#" not in s.get("keys", "") else FAIL, s.get("keys", ""))
    try:
        replay, wrongkey = desk.security_probe()
        report.add(area, "replay rejected", PASS if replay else FAIL)
        report.add(area, "wrong key rejected", PASS if wrongkey else FAIL)
    except Exception as e:
        report.add(area, "security probe", FAIL, str(e))
    try:
        desk.call({"op": "arm", "on": False})
        r = desk.call({"op": "move", "dir": "up", "ms": 100})
        desk.call({"op": "arm", "on": True})
        report.add(area, "child lock refuses movement", PASS if r.get("ok") is False else FAIL, str(r.get("err")))
    except Exception as e:
        report.add(area, "child lock", FAIL, str(e))
    try:
        log = desk.call({"op": "log", "since": 0})
        lines = [f"{l['t'] / 1000:8.1f}s {'EWID'[l['l']]} {l['m']}" for l in log["lines"]]
        report.raw(f"desk-{host}-log", lines)
        errs = [l for l in lines if " E " in l[:14]]
        report.add(area, "desk log has no errors", FAIL if errs else PASS, f"{len(lines)} lines", log=errs)
        raw = desk.call({"op": "raw"})
        report.add(area, "raw capture of the handset line", INFO, "frames the module saw", log=(raw.get("text") or "(nothing)").splitlines())
    except Exception as e:
        report.add(area, "desk log", FAIL, str(e))

    if not active:
        report.add(area, "movement tests", SKIP, "run again with --active (the desk WILL move a little; keep the area clear)")
        return
    # --- active: the desk really moves ---
    def nudge(direction):
        before = desk.call({"op": "state"})
        desk.call({"op": "move", "dir": direction, "ms": 450})
        time.sleep(1.4)
        after = desk.call({"op": "state"})
        if "#" in after["keys"][:2]:
            desk.call({"op": "stop"})
            return False, "key stayed pressed after the keep-alive ended: SAFETY FAILURE"
        return True, f"{before['h']} -> {after['h']} cm"
    for d in ("up", "down"):
        ok, detail = nudge(d)
        if not ok:
            report.add(area, f"nudge {d} dead-man", FAIL, detail)
            continue
        report.add(area, f"nudge {d}", PASS if ask(f"Did the desk move {d} a little and stop by itself? ({detail})") else FAIL, detail)
    for _ in range(4):
        desk.call({"op": "move", "dir": "up", "ms": 700})
        time.sleep(0.25)
    desk.call({"op": "stop"})
    time.sleep(0.3)
    left = desk.call({"op": "state"})["keys"]
    report.add(area, "STOP releases the keys", PASS if "#" not in left else FAIL, left)
    desk.call({"op": "move", "dir": "down", "ms": 700})
    time.sleep(0.4)
    desk.call({"op": "stop"})
    for key in ("s", "x"):
        desk.call({"op": "tap", "key": key, "ms": 150})
        time.sleep(0.6)
        report.add(area, f"key {key.upper()}", PASS if ask(f"Did the handset react to the {key.upper()} key (display / beep)? Answer no if nothing visible happened.") else INFO, "")
