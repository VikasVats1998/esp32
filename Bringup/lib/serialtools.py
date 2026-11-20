"""Serial helpers: find boards, reset them, read their log, send console commands."""
import re
import time

from .projects import BAD_PATTERNS


def _serial():
    try:
        import serial
        import serial.tools.list_ports
        return serial
    except ImportError:
        raise SystemExit("pyserial is missing: run setup.sh (Linux/Mac) or setup.ps1 (Windows), or: pip install pyserial")


def list_ports():
    s = _serial()
    out = []
    for p in s.tools.list_ports.comports():
        espressif = p.vid == 0x303A
        out.append(dict(device=p.device, desc=p.description or "", vid=p.vid, pid=p.pid, espressif=espressif, serial=p.serial_number or ""))
    return out


def open_port(device, baud=115200, timeout=0.2):
    s = _serial()
    ser = s.Serial()
    ser.port = device
    ser.baudrate = baud
    ser.timeout = timeout
    ser.dtr = False
    ser.rts = False
    ser.open()
    return ser


def hard_reset(ser):
    """Reset the chip like esptool does (RTS pulse). Harmless on boards that ignore it."""
    try:
        ser.dtr = False
        ser.rts = True
        time.sleep(0.12)
        ser.rts = False
        time.sleep(0.05)
    except Exception:
        pass


def read_lines(ser, seconds, stop_when=None):
    """Reads for `seconds` (or until stop_when(line) is true). Returns a list of text lines."""
    lines, buf, end = [], b"", time.time() + seconds
    while time.time() < end:
        try:
            chunk = ser.read(256)
        except Exception:
            break
        if chunk:
            buf += chunk
            while b"\n" in buf:
                raw, buf = buf.split(b"\n", 1)
                line = raw.decode("utf-8", errors="replace").rstrip("\r")
                if line:
                    lines.append(line)
                    if stop_when and stop_when(line):
                        return lines
    if buf.strip():
        lines.append(buf.decode("utf-8", errors="replace").strip())
    return lines


def command(ser, cmd, wait=1.0, quiet=0.35):
    """Sends a console command and returns the answer lines. Stops when the line has been quiet for `quiet` s."""
    try:
        ser.reset_input_buffer()
    except Exception:
        pass
    ser.write((cmd + "\n").encode())
    ser.flush()
    lines, buf, start, last = [], b"", time.time(), time.time()
    while time.time() - start < wait + 4:
        chunk = ser.read(256)
        if chunk:
            last = time.time()
            buf += chunk
            while b"\n" in buf:
                raw, buf = buf.split(b"\n", 1)
                line = raw.decode("utf-8", errors="replace").rstrip("\r")
                if line:
                    lines.append(line)
        elif lines and time.time() - last > quiet and time.time() - start > 0.3:
            break
        elif not lines and time.time() - start > wait:
            break
    return lines


def find_problems(lines):
    """Returns (description, line) for every line that looks like trouble."""
    found = []
    for line in lines:
        for pat, desc in BAD_PATTERNS:
            if re.search(pat, line):
                found.append((desc, line))
                break
    return found


def diag_items(lines):
    """Parses '[diag] name: ok ...' / '[diag] name: FAILED ...' lines into {name: (ok, detail)}."""
    items = {}
    for line in lines:
        m = re.search(r"\[diag\] (\w+): (ok|FAILED)\s*(.*)", line)
        if m:
            items[m.group(1)] = (m.group(2) == "ok", m.group(3).strip())
    return items
