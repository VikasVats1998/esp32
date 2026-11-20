"""Tests of Chiku / Jinu over their HTTP API (docs: RobotApp/docs/API.md). Standard library only."""
import json
import time
import urllib.error
import urllib.parse
import urllib.request

from .report import FAIL, INFO, PASS, SKIP

MOODS = ["normal", "happy", "sad", "sleepy", "angry", "surprised", "dizzy", "love", "thinking", "listening",
         "jealous", "sulk", "shy", "excited", "scared", "cry"]


class Robot:
    def __init__(self, host, timeout=4.0):
        self.host, self.timeout = host, timeout

    def call(self, method, path, params=None, body=None):
        url = f"http://{self.host}{path}"
        if params:
            url += "?" + urllib.parse.urlencode(params)
        data = json.dumps(body).encode() if body is not None else (b"" if method == "POST" else None)
        req = urllib.request.Request(url, data=data, method=method, headers={"Content-Type": "application/json"})
        try:
            with urllib.request.urlopen(req, timeout=self.timeout) as r:
                text = r.read().decode("utf-8", "replace")
                return r.status, text
        except urllib.error.HTTPError as e:
            return e.code, e.read().decode("utf-8", "replace")

    def json(self, path, params=None):
        status, text = self.call("GET", path, params)
        if status != 200:
            raise RuntimeError(f"GET {path} -> HTTP {status} {text[:80]}")
        return json.loads(text)

    def post(self, path, params=None, body=None):
        status, text = self.call("POST", path, params, body)
        if status != 200:
            raise RuntimeError(f"POST {path} -> HTTP {status} {text[:80]}")
        try:
            return json.loads(text)
        except ValueError:
            return {"ok": True}


def run(report, host, ask, active=False, kind=None, simulate=False):
    """ask(question) -> True/False (the person answers what they saw/heard/felt)."""
    area = f"robot {host}"
    r = Robot(host)
    try:
        st = r.json("/api/state")
    except Exception as e:
        report.add(area, "reachable", FAIL, str(e))
        return
    kind = st.get("robot", kind)
    report.add(area, "reachable", PASS, f"{kind} fw {st.get('fw')} api {st.get('api')} up {st.get('uptime')} s")
    report.add(area, "Wi-Fi signal", PASS if st.get("rssi", -100) > -75 else FAIL, f"{st.get('rssi')} dBm")
    report.add(area, "memory", PASS if st.get("heap", 0) > 40000 else FAIL, f"heap {st.get('heap', 0) // 1024} KB" + (f", PSRAM {st['psram'] // 1024} KB" if "psram" in st else ""))
    report.add(area, "audio chip", PASS if st.get("audio") else FAIL)
    report.add(area, "touch pad expander", PASS if st.get("padsOnline") else FAIL, "PCF8574 answers" if st.get("padsOnline") else "PCF8574 missing (check wiring / address 0x20)")
    if "pca" in st:
        report.add(area, "servo driver", PASS if st["pca"] else FAIL, "PCA9685")
    if st.get("imu"):
        im = st["imu"]
        mag = (im["x"] ** 2 + im["y"] ** 2 + im["z"] ** 2) ** 0.5
        report.add(area, "IMU gravity", PASS if im.get("ok") and 0.8 < mag < 1.2 else FAIL, f"{mag:.2f} g (expect 1.0)")
    d = st.get("dist", -1)
    report.add(area, "sonar", PASS if 2 <= d <= 400 else FAIL, f"{d} cm" if d >= 0 else "no echo")
    if st.get("cam"):
        report.add(area, "camera link", PASS if st["cam"].get("alive") else FAIL, str(st["cam"].get("ip") or "offline"))
    if st.get("batt") is not None:
        report.add(area, "battery", PASS if st["batt"] >= 0 else FAIL, f"{st['batt']}%" if st["batt"] >= 0 else "no battery reported")

    # boot self check of the firmware itself
    try:
        diag = r.json("/api/diag")
        bad = [i for i in diag["items"] if not i["ok"]]
        report.add(area, "firmware self check", FAIL if bad else PASS, "; ".join(f"{i['name']}: {i['detail']}" for i in bad) if bad else f"{len(diag['items'])} checks ok")
    except Exception as e:
        report.add(area, "firmware self check", FAIL, str(e))

    # the robot's own log: errors?
    try:
        lg = r.json("/api/log", {"since": 0})
        lines = [f"{l['t'] / 1000:8.1f}s {'EWID'[l['l']]} {l['m']}" for l in lg["lines"]]
        errors = [l for l in lines if " E " in l[:14]]
        report.raw(f"robot-{host}-log", lines)
        report.add(area, "robot log has no errors", FAIL if errors else PASS, f"{len(errors)} error line(s)" if errors else f"{len(lines)} lines", log=errors)
    except Exception as e:
        report.add(area, "robot log", FAIL, str(e))

    # --- safe commands (nothing moves) ---
    try:
        r.post("/api/mode", {"m": "manual"})
        ok = r.json("/api/state")["mode"] == "manual"
        report.add(area, "mode switch (manual)", PASS if ok else FAIL)
        r.post("/api/mood", {"m": 1, "hold": 1500})
        report.add(area, "face command accepted", PASS)
        r.post("/api/sim", {"pad": 4, "g": 2})
        time.sleep(0.4)
        ev = r.json("/api/events", {"since": 0})["events"]
        report.add(area, "simulated pad event recorded", PASS if any(e["type"] == "pad" and e["a"] == 4 for e in ev) else FAIL)
    except Exception as e:
        report.add(area, "commands", FAIL, str(e))

    if not active:
        report.add(area, "active tests (display, sound, servos, touch)", SKIP, "run again with --active (robot on a stand!)")
        return

    # --- active tests: you watch / listen / touch ---
    def confirm(name, question, action):
        try:
            action()
        except Exception as e:
            report.add(area, name, FAIL, str(e))
            return
        report.add(area, name, PASS if ask(question) else FAIL, "confirmed by you" if True else "")

    def colours():
        for p in range(1, 7):
            r.post("/api/display", {"p": p})
            time.sleep(1.1)
        r.post("/api/display", {"p": 0})
    confirm("display colours", "Did you see red, green, blue, white, black and a smooth grey ramp (no dead pixels)?", colours)

    def faces():
        for i, _ in enumerate(MOODS):
            r.post("/api/mood", {"m": i, "hold": 0})
            time.sleep(1.3)
        r.post("/api/mood", {"m": 0, "hold": 0})
    confirm("all 16 faces", "Were all 16 faces drawn correctly and smoothly?", faces)

    def sound():
        for hz in (262, 440, 880, 1760):
            r.post("/api/beep", {"hz": hz, "ms": 450})
            time.sleep(0.7)
        r.post("/api/say", {"key": "hello"})
        time.sleep(2.5)
    confirm("sound and voice", "Did you hear 4 rising tones and then a voice saying Hello (or a babble)?", sound)

    n = len(st.get("servo", []))

    def servos():
        for ch in range(n):
            for deg in (90, 105, 75, 90):
                r.post("/api/servo/set", {"ch": ch, "deg": deg, "speed": 50})
                time.sleep(0.9)
    if n:
        confirm("servos move", f"Did all {n} servos swing slightly one after another, smoothly and without buzzing?", servos)
        r.post("/api/servo/release")

    # touch pads: ask the person to touch each pad, watch the state bits
    names = ["pad 0 (ear/head L)", "pad 1 (ear/head R)", "pad 2 (leg L)", "pad 3 (leg R)", "pad 4 (body)"]
    for i, nm in enumerate(names):
        print(f"    touch {nm} now (15 s) ...", flush=True)
        if simulate:                                   # dry run against a simulator: stand in for the person
            r.post("/api/sim", {"pad": i, "g": 1})
        end, seen = time.time() + 15, False
        base = r.json("/api/events", {"since": 0})["next"]
        while time.time() < end and not seen:
            if r.json("/api/state")["pads"] & (1 << i) or any(e["type"] == "pad" and e["a"] == i for e in r.json("/api/events", {"since": base})["events"]):
                seen = True
            time.sleep(0.15)
        report.add(area, f"touch {nm}", PASS if seen else FAIL, "" if seen else "no touch seen in 15 s")
    if simulate:
        report.add(area, "sonar near and far", SKIP, "needs a hand (not simulated)")
        r.post("/api/mode", {"m": "auto"})
        return
    print("    wave your hand 10 cm in front of the sonar, then away (20 s) ...", flush=True)
    end, near, far = time.time() + 20, False, False
    while time.time() < end and not (near and far):
        dd = r.json("/api/state")["dist"]
        near |= 0 < dd < 15
        far |= dd > 40 and near
        time.sleep(0.2)
    report.add(area, "sonar near and far", PASS if near and far else FAIL, "saw both" if near and far else f"near={near} far={far}")
    r.post("/api/mode", {"m": "auto"})
