"""The boards of this repository: where the firmware is, how to flash it, what a healthy boot looks like."""
import os

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))

PROJECTS = {
    "chiku": dict(
        title="Chiku head (ESP32-S3 AMOLED)", folder="Chiku", env="head", fs=True, baud=115200,
        banner=r"\[boot\] chiku fw",
        # hardware that must answer when everything is wired; shown as PASS/FAIL per item
        diag=["display", "touch", "imu", "rtc", "pmu", "psram", "audio", "servo_driver", "pad_expander", "filesystem"]),
    "chiku-cam": dict(
        title="Chiku ESP32-CAM", folder="Chiku", env="cam", fs=False, baud=115200, banner=None, manual_flash=True),
    "jinu": dict(
        title="Jinu (ESP32-C6 LCD 1.47)", folder="Jinu", env="jinu", fs=True, baud=115200,
        banner=r"\[boot\] jinu fw", diag=["display", "audio", "pad_expander", "filesystem"]),
    "desk": dict(
        title="Desk module (ESP32-S3 Super Mini)", folder="Desk", env="desk", fs=False, baud=115200,
        banner=r"\[boot\] desk module fw", diag=[], expect=[r"\[link\] pairing window open"]),
    "gimbal": dict(
        title="Gimbal module (ESP32-S3 Super Mini in the gimbal)", folder="Gimbal", env="gimbal", fs=False, baud=115200,
        banner=r"\[boot\] gimbal module fw", diag=[], expect=[r"\[link\] radio channel", r"\[storage\] boot"]),
    "remote": dict(
        title="Gimbal remote (ESP32-S3 Super Mini)", folder="Remote", env="remote", fs=False, baud=115200,
        banner=r"\[boot\] gimbal remote fw", diag=[], expect=[r"\[inputs\] stick centre"]),
    "probe": dict(
        title="Probe (measuring tool)", folder="Probe", env="probe", fs=False, baud=115200, banner=r"probe ready", diag=[]),
}

# Patterns that always mean trouble in a boot log
BAD_PATTERNS = [
    (r"Guru Meditation", "the chip crashed (Guru Meditation)"),
    (r"abort\(\) was called", "abort() was called"),
    (r"Backtrace:", "crash backtrace"),
    (r"Brownout", "brown-out: the power supply is too weak"),
    (r"rst:0x[0-9a-f]+ \(WDT|task_wdt|Task watchdog", "watchdog reset"),
    (r"\bE \[", "an ERROR line in the log"),
    (r"FAILED", "something reported FAILED"),
]
