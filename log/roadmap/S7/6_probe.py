"""S7.6 development probe: one live session of simgolf_port.exe driven by real posted window messages (app_drive.py).

    ~/bin/py -3.12 log/roadmap/S7/6_probe.py [--exe <simgolf_port.exe>] [--out <dir>] [--frames N] [extra app args...]

Starts the app (96 dpi, --cursor message), waits for the title menu (shown.tsv stops growing), hovers two points, clicks
Sandbox (130, 445) and Monterey (270, 32), lets N frame heads pass, closes the window and prints the session summary.
"""
from __future__ import annotations

import argparse
import json
import pathlib
import shutil
import sys
import tempfile
import time

ROOT = pathlib.Path(__file__).resolve().parents[3]
MIRROR = ROOT / "port" / "build" / "S7_6_head"
sys.path.insert(0, str(MIRROR / "port" / "app" / "tools"))
import app_drive  # noqa: E402


def lines(p: pathlib.Path) -> list[str]:
    return p.read_text().splitlines()[1:] if p.exists() else []


def wait_quiet(p: pathlib.Path, quiet: float, timeout: float, minimum: int = 1) -> int:
    end = time.time() + timeout
    last, t_last = -1, time.time()
    while time.time() < end:
        n = len(lines(p))
        if n != last:
            last, t_last = n, time.time()
        elif n >= minimum and time.time() - t_last >= quiet:
            return n
        time.sleep(0.05)
    return last


def frames(rec: pathlib.Path) -> list[list[int]]:
    p = rec / "frames.tsv"
    return [[int(x) for x in l.split("\t")] for l in p.read_text().splitlines()] if p.exists() else []


def cpu_seconds(pid: int) -> float:
    import ctypes as C
    import ctypes.wintypes as W
    k = C.WinDLL("kernel32", use_last_error=True)
    k.OpenProcess.restype = W.HANDLE
    k.OpenProcess.argtypes = [W.DWORD, W.BOOL, W.DWORD]
    k.GetProcessTimes.argtypes = [W.HANDLE] + [C.POINTER(W.FILETIME)] * 4
    k.CloseHandle.argtypes = [W.HANDLE]
    h = k.OpenProcess(0x1000, False, pid)  # PROCESS_QUERY_LIMITED_INFORMATION
    ft = [W.FILETIME() for _ in range(4)]
    k.GetProcessTimes(h, *[C.byref(f) for f in ft])
    k.CloseHandle(h)
    v = lambda f: ((f.dwHighDateTime << 32) | f.dwLowDateTime) / 1e7
    return v(ft[2]) + v(ft[3])


def main(argv=None) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", default=str(MIRROR / "port" / "build" / "S7_6" / "app" / "Release" / "simgolf_port.exe"))
    ap.add_argument("--out")
    ap.add_argument("--frames", type=int, default=60)
    ap.add_argument("rest", nargs=argparse.REMAINDER)
    a = ap.parse_args(argv)
    out = pathlib.Path(a.out) if a.out else pathlib.Path(tempfile.mkdtemp(prefix="s76_probe_"))
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)
    args = ["--install", str(ROOT / "original"), "--data", str(out / "data"), "--record-dir", str(out / "rec"),
            "--no-activate", "--dpi", "96", "--cursor", "message", *[x for x in a.rest if x != "--"]]
    t0 = time.time()
    app = app_drive.launch(a.exe, args, cwd=str(MIRROR))
    rec = app.record
    try:
        n = wait_quiet(rec / "shown.tsv", 1.5, 120)
        print(f"title: {n} frames shown after {time.time() - t0:.1f} s", flush=True)
        c0, w0 = cpu_seconds(app.proc.pid), time.time()
        time.sleep(10)
        c1, w1 = cpu_seconds(app.proc.pid), time.time()
        print(f"title idle CPU: {100 * (c1 - c0) / (w1 - w0):.2f} % of one core over {w1 - w0:.1f} s", flush=True)
        for x, y in ((400, 445), (650, 445), (130, 445)):
            app.move(x, y)
            time.sleep(0.4)
        print(f"hover: {len(lines(rec / 'shown.tsv'))} frames shown", flush=True)
        app.click(130, 445)
        n = wait_quiet(rec / "shown.tsv", 1.5, 60)
        print(f"location: {n} frames shown", flush=True)
        app.click(270, 32)
        end = time.time() + 120
        while time.time() < end and len(frames(rec)) < a.frames:
            time.sleep(0.2)
        fr = frames(rec)
        print(f"frames: {len(fr)}", flush=True)
    finally:
        rc = app.close()
    s = json.loads((rec / "session.json").read_text())
    print(json.dumps(s, indent=1)[:3000])
    fr = frames(rec)
    if len(fr) > 2:
        d = [fr[k + 1][1] - fr[k][1] for k in range(len(fr) - 1)]
        print(f"period over {len(d)} frames: min {min(d)} max {max(d)} mean {sum(d) / len(d):.1f}")
    print("rc", rc, "out", out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
