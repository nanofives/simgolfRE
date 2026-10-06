"""SimGolf runtime harness: spawn the game (windowed, under Frida), find its window,
screenshot, send input, and kill ONLY the PID we spawned.

    from re_tools.game import Game   (or: sys.path.insert(0, "re/tools"); from game import Game)
    with Game() as g:
        g.wait_window()
        g.shot("out.png")
        g.click(400, 300)          # game coords (800x600 space)
        g.key(0x1B)                # VK_ESCAPE

Hygiene rules (same as Mashed): track the PID you spawn, kill only that PID, never by name.
"""
from __future__ import annotations

import ctypes
import os
import ctypes.wintypes as wt
import pathlib
import threading
import time

import frida
from PIL import Image, ImageGrab

ROOT = pathlib.Path(__file__).resolve().parents[2]
# SIMGOLF_GAME_DIR: run another copy of the install (parallel A/B, CLAUDE.md "parallel C3"); default original\.
GAME_DIR = pathlib.Path(os.environ.get("SIMGOLF_GAME_DIR") or ROOT / "original")
JS_DIR = ROOT / "re" / "frida" / "js"
DEFAULT_EXE = "golf_clean.exe"
GAME_W, GAME_H = 800, 600

user32 = ctypes.windll.user32
try:
    user32.SetProcessDPIAware()
except Exception:
    pass

WM_KEYDOWN, WM_KEYUP = 0x0100, 0x0101
WM_MOUSEMOVE, WM_LBUTTONDOWN, WM_LBUTTONUP = 0x0200, 0x0201, 0x0202
WM_RBUTTONDOWN, WM_RBUTTONUP = 0x0204, 0x0205
MK_LBUTTON, MK_RBUTTON = 0x1, 0x2


def _enum_windows(pid: int, cls: str | None = None):
    out = []

    @ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)
    def cb(h, _):
        p = wt.DWORD()
        user32.GetWindowThreadProcessId(h, ctypes.byref(p))
        if p.value == pid and user32.IsWindowVisible(h):
            buf = ctypes.create_unicode_buffer(256)
            user32.GetClassNameW(h, buf, 256)
            if cls is None or buf.value == cls:
                out.append(h)
        return True

    user32.EnumWindows(cb, 0)
    return out


class Game:
    """One spawned game process. Context manager kills it on exit."""

    def __init__(self, exe: str = DEFAULT_EXE, windowed: bool = True, extra_js: list[str] | None = None,
                 args: list[str] | None = None, on_message=None, env: dict | None = None,
                 coverage: str | pathlib.Path | None = None):
        self.exe = exe
        self.windowed = windowed
        self.env = env or {}
        if coverage:  # entries file: "module\taddr" per line (see shim/src/coverage.cpp)
            self.env["SIMGOLF_COVERAGE"] = str(coverage)
        self.extra_js = extra_js or []
        self.args = args or []
        self.on_message = on_message
        self.pid: int | None = None
        self.session = None
        self.scripts = []
        self.messages: list = []
        self.detached = threading.Event()
        self.detach_reason = None

    # ---------- lifecycle ----------
    @staticmethod
    def running_instances():
        """PIDs of any SimGolf game process (any exe name) that owns a JackalClass window."""
        found = []

        @ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)
        def cb(h, _):
            buf = ctypes.create_unicode_buffer(64)
            user32.GetClassNameW(h, buf, 64)
            if buf.value == "JackalClass":
                p = wt.DWORD()
                user32.GetWindowThreadProcessId(h, ctypes.byref(p))
                found.append(p.value)
            return True

        user32.EnumWindows(cb, 0)
        return sorted(set(found))

    def start(self) -> "Game":
        # The game itself has no single-instance guard (log/single_instance_notes.md, 2026-10-07: three copies, two in
        # the same folder, ran side by side). This refusal protects other sessions' runs; SIMGOLF_ALLOW_MULTI=1
        # (parallel A/B from separate install copies) skips it.
        others = [] if os.environ.get("SIMGOLF_ALLOW_MULTI") == "1" else self.running_instances()
        if others:
            # Never kill these: they may belong to another session.
            raise RuntimeError(f"SimGolf already running (PIDs {others}); the game is single-instance")
        # Windowed mode / Bink fix live in the native shim (original/winmm.dll); env vars override its ini.
        # Virtual cursor parked in the top-left corner from the first frame (no hover anywhere); the
        # user's real mouse never reaches the game.
        # __COMPAT_LAYER: original\golf_clean.exe has the DWM8And16BitMitigation compatibility layer in the registry
        # (HKCU/HKLM AppCompatFlags\Layers, keyed by its path); a copy at any other path does not, its 16-bit mode
        # set fails and the game exits with code 0 right after creating its window. Passing the layer in the
        # environment applies it to the spawned process wherever the install lives (2026-10-07).
        env = {"SIMGOLF_WINDOWED": "1" if self.windowed else "0", "SIMGOLF_VCURSOR": "2,2",
               "__COMPAT_LAYER": "DWM8And16BitMitigation", **self.env}
        self.pid = frida.spawn([str(GAME_DIR / self.exe), *self.args], cwd=str(GAME_DIR), env=env)
        self.session = frida.attach(self.pid)
        self.session.on("detached", self._on_detached)
        sources = ["harness.js"] + self.extra_js
        for name in sources:
            path = pathlib.Path(name)
            src = (path if path.is_absolute() else JS_DIR / name).read_text()
            sc = self.session.create_script(src)
            sc.on("message", self._on_msg)
            sc.load()
            self.scripts.append(sc)
        frida.resume(self.pid)
        return self

    def _on_msg(self, m, data):
        self.messages.append(m.get("payload", m))
        if self.on_message:
            self.on_message(m, data)

    def _on_detached(self, reason, crash):
        self.detach_reason = (reason, crash)
        self.detached.set()

    @property
    def alive(self) -> bool:
        return not self.detached.is_set()

    def kill(self):
        if self.pid and self.alive:
            try:
                frida.kill(self.pid)
            except Exception:
                pass
        self.detached.wait(5)

    def __enter__(self):
        return self.start()

    def __exit__(self, *exc):
        self.kill()

    # ---------- window ----------
    def hwnd(self):
        if not self.pid:
            return None
        w = _enum_windows(self.pid, "JackalClass")
        return w[0] if w else None

    def wait_window(self, timeout: float = 20) -> int:
        t0 = time.time()
        while time.time() - t0 < timeout:
            h = self.hwnd()
            if h:
                return h
            if not self.alive:
                raise RuntimeError(f"game died before window appeared: {self.detach_reason}")
            time.sleep(0.2)
        raise TimeoutError("JackalClass window did not appear")

    def client_box(self):
        h = self.hwnd()
        if not h:
            raise RuntimeError(f"no game window (pid {self.pid}, alive={self.alive})")
        rc = wt.RECT()
        user32.GetClientRect(h, ctypes.byref(rc))
        pt = wt.POINT(0, 0)
        user32.ClientToScreen(h, ctypes.byref(pt))
        return pt.x, pt.y, pt.x + rc.right, pt.y + rc.bottom

    def window_style(self) -> int:
        return user32.GetWindowLongW(self.hwnd(), -16) & 0xFFFFFFFF

    def shot(self, out=None, size=(GAME_W, GAME_H)):
        """Capture the game window's own surface (see _capture), resized to game space."""
        img = self._capture()
        if size and img.size != size:
            img = img.resize(size)
        if out:
            img.save(out)
        return img

    def _capture(self):
        """BitBlt from the window DC. Under DWM this reads the window's redirection surface: correct while
        other windows cover the game (verified with an opaque topmost window on top), and 0/100 black frames
        at the menu. PrintWindow(PW_RENDERFULLCONTENT) was tried first and returned black ~8-18% of the time
        on this legacy GDI window."""
        h = self._hwnd_or_raise()
        rc = wt.RECT()
        user32.GetClientRect(h, ctypes.byref(rc))
        w, hgt = rc.right, rc.bottom
        gdi32 = ctypes.windll.gdi32
        hdc = user32.GetDC(h)
        mdc = gdi32.CreateCompatibleDC(hdc)
        bmp = gdi32.CreateCompatibleBitmap(hdc, w, hgt)
        gdi32.SelectObject(mdc, bmp)
        try:
            gdi32.BitBlt(mdc, 0, 0, w, hgt, hdc, 0, 0, 0x00CC0020)  # SRCCOPY

            class BMIH(ctypes.Structure):
                _fields_ = [("biSize", wt.DWORD), ("biWidth", wt.LONG), ("biHeight", wt.LONG), ("biPlanes", wt.WORD),
                            ("biBitCount", wt.WORD), ("biCompression", wt.DWORD), ("biSizeImage", wt.DWORD),
                            ("biXPelsPerMeter", wt.LONG), ("biYPelsPerMeter", wt.LONG), ("biClrUsed", wt.DWORD),
                            ("biClrImportant", wt.DWORD)]
            bi = BMIH(ctypes.sizeof(BMIH), w, -hgt, 1, 32, 0, 0, 0, 0, 0, 0)
            buf = ctypes.create_string_buffer(w * hgt * 4)
            gdi32.GetDIBits(mdc, bmp, 0, hgt, buf, ctypes.byref(bi), 0)
            img = Image.frombuffer("RGB", (w, hgt), buf, "raw", "BGRX", 0, 1)
            # The game is DPI-unaware: its unscaled surface sits in the top-left corner of the
            # physical-size buffer (800x600 inside 1000x750 at 125%). Crop to the logical size.
            scale = user32.GetDpiForSystem() / 96.0
            lw, lh = round(w / scale), round(hgt / scale)
            return img.crop((0, 0, lw, lh)) if (lw, lh) != (w, hgt) else img
        finally:
            gdi32.DeleteObject(bmp)
            gdi32.DeleteDC(mdc)
            user32.ReleaseDC(h, hdc)

    # ---------- input (posted to the window; does not need focus) ----------
    def _lp(self, x, y):
        """lParam for posted mouse messages. Windows rescales posted mouse coordinates from this DPI-aware
        process to the DPI-unaware game window (observed: (115,495) arrived as (92,396) at 125%), so send
        physical coordinates; the in-game UI hit-tests on lParam, the main menu on GetCursorPos."""
        s = user32.GetDpiForSystem() / 96.0
        return (int(round(y * s)) << 16) | (int(round(x * s)) & 0xFFFF)

    def _hwnd_or_raise(self):
        h = self.hwnd()
        if not h:
            raise RuntimeError(f"no game window (pid {self.pid}, alive={self.alive})")
        return h

    @property
    def harness(self):
        return self.scripts[0].exports_sync

    def cov_phase(self, n: int) -> int:
        """Switch the coverage phase bit and re-arm every entry. Returns the number armed."""
        return self.harness.cov_phase(n)

    def cov_dump(self, path) -> int:
        """Write module/addr/mask TSV of every entry that ran. Returns the row count."""
        return self.harness.cov_dump(str(path))

    def shim_info(self):
        return self.harness.shim_info()

    def move(self, x, y):
        """Point the shim's virtual cursor at (x, y) in game space; the real mouse is untouched."""
        self.harness.set_cursor(int(x), int(y))
        user32.PostMessageW(self._hwnd_or_raise(), WM_MOUSEMOVE, 0, self._lp(x, y))

    def click(self, x, y, right=False, hold=0.15, settle=0.35):
        """Move the virtual cursor, hold the virtual button for a few frames, release. Both input paths
        are driven: window messages (main menu) and the GetKeyboardState/GetAsyncKeyState button bit
        (in-game UI), so the click works on every screen without touching the real mouse."""
        h = self._hwnd_or_raise()
        self.move(x, y)
        time.sleep(settle)  # several game ticks: hover state must follow the cursor before the button goes down
        lp = self._lp(x, y)
        vk = 0x02 if right else 0x01
        down, up, mk = (WM_RBUTTONDOWN, WM_RBUTTONUP, MK_RBUTTON) if right else (WM_LBUTTONDOWN, WM_LBUTTONUP, MK_LBUTTON)
        user32.PostMessageW(h, WM_MOUSEMOVE, 0, lp)
        self.harness.vkey(vk, 2)
        user32.PostMessageW(h, down, mk, lp)
        time.sleep(hold)
        self.harness.vkey(vk, 1)
        user32.PostMessageW(h, up, 0, lp)
        time.sleep(0.1)

    def drag(self, x0, y0, x1, y1, steps=8, step_time=0.05):
        """Press at (x0,y0), move to (x1,y1), release (building tees/greens/fairways)."""
        h = self._hwnd_or_raise()
        self.move(x0, y0)
        time.sleep(0.35)
        self.harness.vkey(0x01, 2)
        user32.PostMessageW(h, WM_LBUTTONDOWN, MK_LBUTTON, self._lp(x0, y0))
        for i in range(1, steps + 1):
            x = x0 + (x1 - x0) * i / steps
            y = y0 + (y1 - y0) * i / steps
            self.move(x, y)
            user32.PostMessageW(h, WM_MOUSEMOVE, MK_LBUTTON, self._lp(x, y))
            time.sleep(step_time)
        time.sleep(0.2)
        self.harness.vkey(0x01, 1)
        user32.PostMessageW(h, WM_LBUTTONUP, 0, self._lp(x1, y1))
        time.sleep(0.1)

    def key(self, vk, hold=0.15, char=None):
        """Key press through every path: WM_KEYDOWN/UP, WM_CHAR (posted keys are not translated reliably;
        the game reads 'f' as a character), and the virtual key-state overlay.
        `char` defaults to the lowercase letter/digit for A-Z/0-9."""
        h = self._hwnd_or_raise()
        if char is None and (0x30 <= vk <= 0x39 or 0x41 <= vk <= 0x5A):
            char = chr(vk).lower()
        self.harness.vkey(vk, 2)
        user32.PostMessageW(h, WM_KEYDOWN, vk, 1)
        if char:
            user32.PostMessageW(h, 0x0102, ord(char), 1)  # WM_CHAR
        time.sleep(hold)
        self.harness.vkey(vk, 0)
        user32.PostMessageW(h, WM_KEYUP, vk, 0xC0000001)
        time.sleep(0.05)
