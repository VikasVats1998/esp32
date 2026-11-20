"""Results and the report files (Markdown for you, a short text to paste to Claude)."""
import datetime
import os
import platform
import sys

PASS, FAIL, SKIP, INFO = "PASS", "FAIL", "SKIP", "INFO"


class Result:
    def __init__(self, area, name, status, detail="", log=None):
        self.area, self.name, self.status, self.detail, self.log = area, name, status, detail, log or []


class Report:
    def __init__(self, root):
        self.stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
        self.dir = os.path.join(root, "reports", self.stamp)
        os.makedirs(os.path.join(self.dir, "raw"), exist_ok=True)
        self.results = []
        self.notes = []

    def add(self, area, name, status, detail="", log=None):
        r = Result(area, name, status, detail, log)
        self.results.append(r)
        mark = {PASS: "PASS", FAIL: "FAIL", SKIP: "skip", INFO: "info"}[status]
        print(f"  [{mark}] {area}: {name}" + (f" - {detail}" if detail else ""), flush=True)
        return r

    def raw(self, name, lines):
        path = os.path.join(self.dir, "raw", name.replace("/", "_") + ".log")
        with open(path, "a", encoding="utf-8") as f:
            f.write("\n".join(lines) + "\n")
        return path

    def note(self, text):
        self.notes.append(text)

    def counts(self):
        c = {PASS: 0, FAIL: 0, SKIP: 0, INFO: 0}
        for r in self.results:
            c[r.status] += 1
        return c

    def write(self):
        c = self.counts()
        head = [
            f"# Bring-up report {self.stamp}",
            f"python {sys.version.split()[0]} on {platform.platform()}",
            f"**{c[PASS]} passed, {c[FAIL]} failed, {c[SKIP]} skipped**",
            "",
        ]
        body = ["| Area | Test | Result | Detail |", "|---|---|---|---|"]
        for r in self.results:
            body.append(f"| {r.area} | {r.name} | {r.status} | {r.detail.replace('|', '/')} |")
        notes = ["", "## Notes", *self.notes] if self.notes else []
        with open(os.path.join(self.dir, "report.md"), "w", encoding="utf-8") as f:
            f.write("\n".join(head + body + notes) + "\n")
        # the short text to paste into the chat: failures first, with their log lines
        lines = [f"BRINGUP {self.stamp}: {c[PASS]} pass, {c[FAIL]} fail, {c[SKIP]} skip"]
        fails = [r for r in self.results if r.status == FAIL]
        for r in fails:
            lines.append(f"FAIL {r.area} / {r.name}: {r.detail}")
            for l in r.log[-12:]:
                lines.append(f"    {l}")
        infos = [r for r in self.results if r.status == INFO]
        for r in infos:
            lines.append(f"INFO {r.area} / {r.name}: {r.detail}")
            for l in r.log[:40]:
                lines.append(f"    {l}")
        lines.append("PASSED: " + ", ".join(f"{r.area}/{r.name}" for r in self.results if r.status == PASS))
        lines.append("SKIPPED: " + ", ".join(f"{r.area}/{r.name}" for r in self.results if r.status == SKIP))
        lines += self.notes
        text = "\n".join(lines) + "\n"
        with open(os.path.join(self.dir, "paste-me.txt"), "w", encoding="utf-8") as f:
            f.write(text)
        with open(os.path.join(os.path.dirname(self.dir), "..", "paste-me.txt"), "w", encoding="utf-8") as f:
            f.write(text)
        return os.path.join(self.dir, "report.md"), os.path.join(self.dir, "paste-me.txt")
