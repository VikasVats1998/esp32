#!/usr/bin/env python3
"""Serves remote_monitor.html and streams the board's serial data to the browser (any browser, e.g. Firefox).
No dependencies (standard library only).   Usage: python3 remote_server.py [port] [http_port]
Then open http://localhost:8765
"""
import glob, os, sys, termios, threading, time, webbrowser
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

here = os.path.dirname(os.path.abspath(__file__))
ports = sorted(glob.glob("/dev/ttyACM*") + glob.glob("/dev/ttyUSB*"))
dev = sys.argv[1] if len(sys.argv) > 1 else (ports[0] if ports else None)
http_port = int(sys.argv[2]) if len(sys.argv) > 2 else 8765
if not dev: sys.exit("No serial port found - plug in the board.")

latest = {"line": "", "n": 0}
cond = threading.Condition()

def open_serial():
    fd = os.open(dev, os.O_RDWR | os.O_NOCTTY)
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
            fd = open_serial(); buf = b""
            while True:
                chunk = os.read(fd, 256)
                if not chunk: break
                buf += chunk
                while b"\n" in buf:
                    line, buf = buf.split(b"\n", 1)
                    with cond:
                        latest["line"] = line.decode(errors="ignore").strip(); latest["n"] += 1
                        cond.notify_all()
        except OSError as e:
            print("serial:", e); time.sleep(1)

class H(BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def do_GET(self):
        if self.path == "/stream":
            self.send_response(200)
            self.send_header("Content-Type", "text/event-stream"); self.send_header("Cache-Control", "no-cache")
            self.end_headers()
            seen = 0
            try:
                while True:
                    with cond:
                        cond.wait_for(lambda: latest["n"] != seen, timeout=2)
                        seen = latest["n"]; line = latest["line"]
                    self.wfile.write(f"data: {line}\n\n".encode()); self.wfile.flush()
            except (BrokenPipeError, ConnectionResetError): pass
        else:
            data = open(os.path.join(here, "remote_monitor.html"), "rb").read()
            self.send_response(200); self.send_header("Content-Type", "text/html; charset=utf-8"); self.end_headers()
            self.wfile.write(data)

threading.Thread(target=reader, daemon=True).start()
url = f"http://localhost:{http_port}"
print(f"Reading {dev} -> {url}  (Ctrl+C to stop)")
threading.Timer(0.5, lambda: webbrowser.open(url)).start()
try: ThreadingHTTPServer(("127.0.0.1", http_port), H).serve_forever()
except KeyboardInterrupt: pass
