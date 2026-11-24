#!/usr/bin/env python3
"""Web UI bridge for OttoCalib: serves otto_ui.html, streams telemetry (SSE) and forwards commands to the board.
Standard library only.   Usage: python3 otto_server.py [serial_port] [http_port]   ->  http://localhost:8766
Close the Arduino Serial Monitor first (only one program can hold the port)."""
import glob, os, sys, termios, threading, time, webbrowser
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

here = os.path.dirname(os.path.abspath(__file__))
ports = sorted(glob.glob("/dev/ttyACM*") + glob.glob("/dev/ttyUSB*"))
dev = sys.argv[1] if len(sys.argv) > 1 else (ports[0] if ports else None)
http_port = int(sys.argv[2]) if len(sys.argv) > 2 else 8766
if not dev: print("No board yet - waiting for it to be plugged in...")

state = {"fd": None, "line": "", "n": 0}
cond = threading.Condition()

def find_dev():
    if len(sys.argv) > 1: return sys.argv[1]
    found = sorted(glob.glob("/dev/ttyACM*") + glob.glob("/dev/ttyUSB*"))
    return found[0] if found else None

def open_serial():
    d = find_dev()
    if not d: raise OSError("board not found (re-plug USB)")
    fd = os.open(d, os.O_RDWR | os.O_NOCTTY)
    a = termios.tcgetattr(fd)
    a[0] = a[1] = a[3] = 0
    a[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
    a[4] = a[5] = termios.B115200
    a[6][termios.VMIN] = 1; a[6][termios.VTIME] = 0
    termios.tcsetattr(fd, termios.TCSANOW, a)
    return fd

def reader():
    while True:
        try:
            fd = state["fd"] = open_serial(); buf = b""; state["err"] = None; print("connected")
            while True:
                chunk = os.read(fd, 512)
                if not chunk: break
                buf += chunk
                while b"\n" in buf:
                    line, buf = buf.split(b"\n", 1)
                    line = line.decode(errors="ignore").strip()
                    if line.startswith("T,") or line.startswith("//") or line.startswith("int ") or line.startswith("bool "):
                        with cond:
                            state["line"] = line; state["n"] += 1; cond.notify_all()
        except OSError as e:
            state["fd"] = None
            if state.get("err") != str(e): print("serial:", e, "- retrying..."); state["err"] = str(e)
            time.sleep(1)

class H(BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def do_GET(self):
        if self.path == "/stream":
            self.send_response(200)
            self.send_header("Content-Type", "text/event-stream"); self.send_header("Cache-Control", "no-cache")
            self.end_headers()
            seen = state["n"]
            try:
                while True:
                    with cond:
                        cond.wait_for(lambda: state["n"] != seen, timeout=2)
                        if state["n"] == seen: line = ""
                        else: seen = state["n"]; line = state["line"]
                    self.wfile.write(f"data: {line}\n\n".encode()); self.wfile.flush()
            except (BrokenPipeError, ConnectionResetError): pass
        else:
            data = open(os.path.join(here, "otto_ui.html"), "rb").read()
            self.send_response(200); self.send_header("Content-Type", "text/html; charset=utf-8"); self.end_headers()
            self.wfile.write(data)
    def do_POST(self):
        body = self.rfile.read(int(self.headers.get("Content-Length", 0))).decode(errors="ignore").strip()
        ok = state["fd"] is not None and (body.startswith("!") and len(body) < 40)
        if ok:
            try: os.write(state["fd"], (body + "\n").encode())
            except OSError: ok = False
        self.send_response(200 if ok else 400); self.end_headers()

threading.Thread(target=reader, daemon=True).start()
url = f"http://localhost:{http_port}"
print(f"Reading {dev} -> {url}  (Ctrl+C to stop)")
threading.Timer(0.5, lambda: webbrowser.open(url)).start()
try: ThreadingHTTPServer(("127.0.0.1", http_port), H).serve_forever()
except KeyboardInterrupt: pass
