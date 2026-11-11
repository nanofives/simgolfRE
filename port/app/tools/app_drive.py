"""Test driver for the app shell (roadmap S7.3): starts simgolf_port.exe, waits for its window, posts REAL window messages to
it (PostMessageW: mouse buttons, mouse moves, keys, characters), captures its client area from the window DC (BitBlt of the
DWM redirection surface, as re/tools/game.py's Game.shot does for the original) and lists the modules loaded in its process.

The driver is per-monitor DPI aware (v2), like the app, so Windows rescales neither the posted coordinates nor the capture:
a point posted at client (x, y) reaches the app as (x, y).

    ~/bin/py -3.12 port/app/tools/app_drive.py --exe <simgolf_port.exe> [--script default] -- <app arguments>

prints one JSON object: the app's READY facts, the posted messages, the module list and the capture's size.
Library use (port/test/test_app_shell.py): launch(), App.capture(), App.post(), App.click(), App.key(), App.close(),
modules(pid).

The app's stderr (from the start) and stdout (after its READY line) are read on threads to their end and kept in the recording
directory as stderr.txt / stdout.txt (App.stderr_path / App.stdout_path, App.stderr_text()): an unread pipe blocks the app once it is
full (P23E fix 7: a run printing many P14 VERIFY lines stopped presenting and was ended by close()). app.proc.stderr / .stdout are
then views of those threads' lines (readline, iteration, read), so a caller that drains them itself reads every line as before.
"""
from __future__ import annotations

import argparse
import ctypes as C
import ctypes.wintypes as W
import json
import os
import pathlib
import subprocess
import sys
import threading
import time

user32 = C.WinDLL("user32", use_last_error=True)
gdi32 = C.WinDLL("gdi32", use_last_error=True)
kernel32 = C.WinDLL("kernel32", use_last_error=True)
psapi = C.WinDLL("psapi", use_last_error=True)

try:
    user32.SetProcessDpiAwarenessContext(C.c_void_p(-4))  # DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
except (AttributeError, OSError):
    pass

WM_KEYDOWN, WM_KEYUP, WM_CHAR = 0x100, 0x101, 0x102
WM_MOUSEMOVE, WM_LBUTTONDOWN, WM_LBUTTONUP, WM_RBUTTONDOWN, WM_RBUTTONUP = 0x200, 0x201, 0x202, 0x204, 0x205
WM_CLOSE = 0x10
MK_LBUTTON, MK_RBUTTON = 1, 2

user32.PostMessageW.argtypes = [W.HWND, W.UINT, W.WPARAM, W.LPARAM]
user32.GetDC.argtypes = [W.HWND]
user32.GetDC.restype = W.HDC
user32.ReleaseDC.argtypes = [W.HWND, W.HDC]
user32.GetClientRect.argtypes = [W.HWND, C.POINTER(W.RECT)]
user32.IsWindow.argtypes = [W.HWND]
user32.MapVirtualKeyW.argtypes = [W.UINT, W.UINT]
gdi32.CreateCompatibleDC.argtypes = [W.HDC]
gdi32.CreateCompatibleDC.restype = W.HDC
gdi32.SelectObject.argtypes = [W.HDC, W.HGDIOBJ]
gdi32.SelectObject.restype = W.HGDIOBJ
gdi32.DeleteObject.argtypes = [W.HGDIOBJ]
gdi32.DeleteDC.argtypes = [W.HDC]
gdi32.BitBlt.argtypes = [W.HDC, C.c_int, C.c_int, C.c_int, C.c_int, W.HDC, C.c_int, C.c_int, W.DWORD]
gdi32.CreateDIBSection.argtypes = [W.HDC, C.c_void_p, W.UINT, C.POINTER(C.c_void_p), W.HANDLE, W.DWORD]
gdi32.CreateDIBSection.restype = W.HBITMAP
gdi32.GdiFlush.restype = W.BOOL
kernel32.OpenProcess.restype = W.HANDLE
kernel32.OpenProcess.argtypes = [W.DWORD, W.BOOL, W.DWORD]
kernel32.CloseHandle.argtypes = [W.HANDLE]
psapi.EnumProcessModulesEx.argtypes = [W.HANDLE, C.POINTER(W.HMODULE), W.DWORD, C.POINTER(W.DWORD), W.DWORD]
psapi.GetModuleFileNameExW.argtypes = [W.HANDLE, W.HMODULE, W.LPWSTR, W.DWORD]

SRCCOPY = 0x00CC0020
CREATE_NO_WINDOW = 0x08000000
NORMAL_PRIORITY_CLASS = 0x00000020


def priority_flags() -> int:
    """roadmap S3.14 (U-1395): the app starts in the Normal priority class, the class an Explorer launch gives it. A test runner
    can run below normal (the machine's test-slot hook puts pytest at BELOW_NORMAL, and a child inherits its parent's class), and
    there the host's ABOVE_NORMAL sim thread (host.cpp, U-1655) ranks below every normal-priority thread on the machine: its
    frame waits ended 1-5 s late under load (log/roadmap/S3/14_notes.md). Decision G10 (re/roadmap/DECISIONS_PENDING.md, 2026-10-28):
    Normal for the app only; pytest and every other test stay at BelowNormal. SG_APP_PRIORITY=inherit keeps the parent's class (the
    switch, and the negative control)."""
    return 0 if os.environ.get("SG_APP_PRIORITY", "") == "inherit" else NORMAL_PRIORITY_CLASS


class BITMAPINFOHEADER(C.Structure):
    _fields_ = [("biSize", W.DWORD), ("biWidth", W.LONG), ("biHeight", W.LONG), ("biPlanes", W.WORD),
                ("biBitCount", W.WORD), ("biCompression", W.DWORD), ("biSizeImage", W.DWORD),
                ("biXPelsPerMeter", W.LONG), ("biYPelsPerMeter", W.LONG), ("biClrUsed", W.DWORD),
                ("biClrImportant", W.DWORD)]


def lparam(x: int, y: int) -> int:
    return ((y & 0xFFFF) << 16) | (x & 0xFFFF)


def modules(pid: int) -> list[str]:
    """Full paths of the modules loaded in process `pid` (EnumProcessModulesEx, all architectures)."""
    h = kernel32.OpenProcess(0x0400 | 0x0010, False, pid)  # PROCESS_QUERY_INFORMATION | PROCESS_VM_READ
    if not h:
        raise OSError(C.get_last_error(), "OpenProcess")
    try:
        arr = (W.HMODULE * 1024)()
        need = W.DWORD(0)
        if not psapi.EnumProcessModulesEx(h, arr, C.sizeof(arr), C.byref(need), 3):
            raise OSError(C.get_last_error(), "EnumProcessModulesEx")
        out = []
        for i in range(min(need.value // C.sizeof(W.HMODULE), 1024)):
            buf = C.create_unicode_buffer(1024)
            psapi.GetModuleFileNameExW(h, arr[i], buf, 1024)
            out.append(buf.value)
        return out
    finally:
        kernel32.CloseHandle(h)


class _Drain:
    """P23E fix 7: the only reader of one of the app's pipes: a thread reads it to its end and keeps every line, in memory and,
    once keep() names a directory, in <dir>/<name> (closed when the pipe ends: the app exited). reader() gives the pipe's
    readline / iteration / read to a caller (every line from the first, in order), so the drivers that drain app.proc.stderr /
    .stdout themselves read the same text as before."""

    def __init__(self, f, name: str):
        self.f, self.name = f, name
        self.cond = threading.Condition()
        self.lines: list[str] = []
        self.out = None
        self.path: pathlib.Path | None = None
        self.done = False
        self.t = threading.Thread(target=self._run, name=f"app_drive {name}", daemon=True)
        self.t.start()

    def _run(self):
        try:
            for line in self.f:
                with self.cond:
                    self.lines.append(line)
                    if self.out:
                        self.out.write(line)
                        self.out.flush()
                    self.cond.notify_all()
        except (OSError, ValueError):
            pass
        finally:
            with self.cond:
                self.done = True
                if self.out:
                    self.out.close()
                    self.out = None
                self.cond.notify_all()

    def keep(self, d: pathlib.Path):
        with self.cond:
            try:
                d.mkdir(parents=True, exist_ok=True)
                self.path = d / self.name
                out = open(self.path, "w", encoding="utf-8", errors="replace")
            except OSError:
                self.path = None
                return
            out.writelines(self.lines)
            out.flush()
            if self.done:
                out.close()
            else:
                self.out = out

    def text(self) -> str:
        with self.cond:
            return "".join(self.lines)

    def join(self, timeout: float):
        self.t.join(timeout)

    def reader(self) -> "_DrainReader":
        return _DrainReader(self)


class _DrainReader:
    """a pipe-like view of a _Drain (readline returns "" at the end, as a text pipe's does)"""

    def __init__(self, d: _Drain):
        self.d, self.pos = d, 0
        self.closed = False

    def readline(self) -> str:
        with self.d.cond:
            while self.pos >= len(self.d.lines) and not self.d.done:
                self.d.cond.wait()
            if self.pos >= len(self.d.lines):
                return ""
            self.pos += 1
            return self.d.lines[self.pos - 1]

    def __iter__(self):
        return iter(self.readline, "")

    def read(self) -> str:
        with self.d.cond:
            while not self.d.done:
                self.d.cond.wait()
            s = "".join(self.d.lines[self.pos:])
            self.pos = len(self.d.lines)
            return s

    def close(self):
        self.closed = True


class App:
    def __init__(self, proc: subprocess.Popen, ready: dict, lines: list[str], err: _Drain | None = None, out: _Drain | None = None):
        self.proc, self.ready, self.lines = proc, ready, lines
        self.hwnd = ready["hwnd"]
        self.posted: list[tuple[int, int, int]] = []
        self._err, self._out = err, out  # P23E fix 7

    @property
    def stderr_path(self) -> pathlib.Path | None:
        return self._err.path if self._err else None

    @property
    def stdout_path(self) -> pathlib.Path | None:
        return self._out.path if self._out else None

    def stderr_text(self) -> str:
        """the app's stderr so far (the tail, when it is long; the whole of it is in stderr_path)"""
        return self._err.text() if self._err else ""

    @property
    def record(self) -> pathlib.Path:
        return pathlib.Path(self.ready["record"])

    def client_size(self) -> tuple[int, int]:
        r = W.RECT()
        user32.GetClientRect(self.hwnd, C.byref(r))
        return r.right - r.left, r.bottom - r.top

    def capture(self):
        """The client area as an h x w x 3 uint8 RGB array (numpy), read from the window DC."""
        import numpy as np
        w, h = self.client_size()
        src = user32.GetDC(self.hwnd)
        mem = gdi32.CreateCompatibleDC(src)
        bih = BITMAPINFOHEADER(C.sizeof(BITMAPINFOHEADER), w, -h, 1, 32, 0, 0, 0, 0, 0, 0)
        bits = C.c_void_p()
        bmp = gdi32.CreateDIBSection(mem, C.byref(bih), 0, C.byref(bits), None, 0)
        old = gdi32.SelectObject(mem, bmp)
        gdi32.BitBlt(mem, 0, 0, w, h, src, 0, 0, SRCCOPY)
        gdi32.GdiFlush()
        raw = C.string_at(bits, w * h * 4)
        gdi32.SelectObject(mem, old)
        gdi32.DeleteObject(bmp)
        gdi32.DeleteDC(mem)
        user32.ReleaseDC(self.hwnd, src)
        a = np.frombuffer(raw, np.uint8).reshape(h, w, 4)
        return a[:, :, 2::-1].copy()  # BGRA -> RGB

    def post(self, msg: int, wp: int, lp: int):
        if not user32.PostMessageW(self.hwnd, msg, wp, lp):
            raise OSError(C.get_last_error(), "PostMessageW")
        self.posted.append((msg, wp & 0xFFFFFFFF, lp & 0xFFFFFFFF))

    def move(self, x: int, y: int):
        self.post(WM_MOUSEMOVE, 0, lparam(x, y))

    def click(self, x: int, y: int, right: bool = False):
        down, up, mk = (WM_RBUTTONDOWN, WM_RBUTTONUP, MK_RBUTTON) if right else (WM_LBUTTONDOWN, WM_LBUTTONUP, MK_LBUTTON)
        self.post(down, mk, lparam(x, y))
        self.post(up, 0, lparam(x, y))

    def key(self, vk: int):
        """A key press as the keyboard driver posts it: WM_KEYDOWN then WM_KEYUP with the scan code (the app's
        TranslateMessage adds the WM_CHAR a character key produces)."""
        scan = user32.MapVirtualKeyW(vk, 0) & 0xFF
        self.post(WM_KEYDOWN, vk, (scan << 16) | 1)
        self.post(WM_KEYUP, vk, (1 << 31) | (1 << 30) | (scan << 16) | 1)

    def char(self, ch: int):
        self.post(WM_CHAR, ch, 1)

    def events(self) -> list[dict]:
        p = self.record / "input.jsonl"
        if not p.exists():
            return []
        return [json.loads(l) for l in p.read_text().splitlines() if l.strip()]

    def wait_events(self, n: int, timeout: float = 20.0) -> list[dict]:
        end = time.time() + timeout
        while time.time() < end:
            ev = self.events()
            if matched(self.posted, ev) >= n:
                return ev
            time.sleep(0.05)
        return self.events()

    def modules(self) -> list[str]:
        return modules(self.proc.pid)

    def close(self, timeout: float = 30.0) -> int:
        if self.proc.poll() is None and user32.IsWindow(self.hwnd):
            user32.PostMessageW(self.hwnd, WM_CLOSE, 0, 0)
        try:
            rc = self.proc.wait(timeout)
        except subprocess.TimeoutExpired:
            self.proc.kill()  # (TerminateProcess(handle, 1): a killed app's code is 1)
            rc = self.proc.wait(10)
        for d in (self._err, self._out):  # P23E fix 7: the pipes' last lines, and their files closed before a caller removes them
            if d:
                d.join(5.0)
        return rc


def matched(posted: list[tuple[int, int, int]], events: list[dict]) -> int:
    """How many of the posted messages appear, in order, among the logged events (other events, such as real mouse moves
    over the window or the WM_CHAR TranslateMessage adds, are skipped)."""
    k = 0
    for e in events:
        if k < len(posted) and (e["msg"], e["wp"], e["lp"]) == posted[k]:
            k += 1
    return k


def launch(exe, args: list[str], cwd=None, timeout: float = 300.0) -> App:
    """Starts the app (no console window) and waits for its READY line: hwnd, client size, dpi, recording directory."""
    proc = subprocess.Popen([str(pathlib.Path(exe).resolve()), *args], cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
                            creationflags=CREATE_NO_WINDOW | priority_flags())
    err = _Drain(proc.stderr, "stderr.txt")  # P23E fix 7: read from the start, kept in the recording directory once it is known
    lines: list[str] = []
    end = time.time() + timeout
    while time.time() < end:
        line = proc.stdout.readline()
        if not line:
            if proc.poll() is not None:
                err.join(5.0)
                raise RuntimeError(f"app exited {proc.returncode}: {err.text()[-2000:]}")
            continue
        lines.append(line.rstrip())
        if line.startswith("READY "):
            f = dict(kv.split("=", 1) for kv in line.split()[1:])
            cw, ch = (int(v) for v in f["client"].split("x"))
            ready = {"hwnd": int(f["hwnd"], 16), "client": (cw, ch), "dpi": int(f["dpi"]), "record": f["record"]}
            out = _Drain(proc.stdout, "stdout.txt")  # P23E fix 7: the rest of stdout, read too
            proc.stdout = out.reader()  # (the drivers that drain app.proc.stdout / .stderr read these views)
            proc.stderr = err.reader()
            if ready["record"]:
                err.keep(pathlib.Path(ready["record"]))
                out.keep(pathlib.Path(ready["record"]))
            return App(proc, ready, lines, err, out)
    proc.kill()
    raise TimeoutError("no READY line from the app")


def default_script(app: App):
    """Every message class of the corpus, at points across the content area: mouse moves, left and right clicks, two key
    presses (a letter, which TranslateMessage turns into a WM_CHAR too, and an arrow key) and one posted WM_CHAR."""
    cw, ch = app.ready["client"]
    for fx, fy in ((0.1, 0.1), (0.5, 0.5), (0.9, 0.8), (0.999, 0.999)):
        x, y = int(cw * fx), int(ch * fy)
        app.move(x, y)
        app.click(x, y)
    app.click(int(cw * 0.3), int(ch * 0.7), right=True)
    app.key(ord("F"))
    app.key(0x26)  # VK_UP
    app.char(ord("x"))


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--exe", required=True)
    ap.add_argument("--script", default="default")
    ap.add_argument("rest", nargs=argparse.REMAINDER)
    a = ap.parse_args(argv)
    args = a.rest[1:] if a.rest[:1] == ["--"] else a.rest
    app = launch(a.exe, args)
    try:
        if a.script == "default":
            default_script(app)
        ev = app.wait_events(len(app.posted))
        mods = app.modules()
        img = app.capture()
    finally:
        rc = app.close()
    print(json.dumps({"ready": app.ready, "posted": app.posted, "matched": matched(app.posted, ev), "events": len(ev),
                      "modules": mods, "capture": list(img.shape), "rc": rc}))
    return 0


if __name__ == "__main__":
    sys.exit(main())
