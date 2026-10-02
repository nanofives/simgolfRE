"""Runtime checks: boot, windowed mode, display untouched, Frida attach/instrumentation, input, in-game.

All tests here launch golf_clean.exe through Frida (spawned suspended, harness agent loaded before the
first instruction) with the shim providing windowed mode. The game is single-instance, so these run
serially and fail fast if another copy is open.
"""
import ctypes
import ctypes.wintypes as wt
import time

import pytest
from PIL import Image

import imgcmp
from conftest import GAME_DIR, GOLDEN

pytestmark = pytest.mark.runtime
user32 = ctypes.windll.user32

WS_POPUP, WS_CAPTION = 0x80000000, 0x00C00000


def _display_mode():
    class DEVMODEW(ctypes.Structure):
        _fields_ = [("dmDeviceName", wt.WCHAR * 32), ("dmSpecVersion", wt.WORD), ("dmDriverVersion", wt.WORD),
                    ("dmSize", wt.WORD), ("dmDriverExtra", wt.WORD), ("dmFields", wt.DWORD),
                    ("dmPosition", wt.POINT), ("dmDisplayOrientation", wt.DWORD), ("dmDisplayFixedOutput", wt.DWORD),
                    ("dmColor", ctypes.c_short), ("dmDuplex", ctypes.c_short), ("dmYResolution", ctypes.c_short),
                    ("dmTTOption", ctypes.c_short), ("dmCollate", ctypes.c_short), ("dmFormName", wt.WCHAR * 32),
                    ("dmLogPixels", wt.WORD), ("dmBitsPerPel", wt.DWORD), ("dmPelsWidth", wt.DWORD),
                    ("dmPelsHeight", wt.DWORD), ("dmDisplayFlags", wt.DWORD), ("dmDisplayFrequency", wt.DWORD)]
    dm = DEVMODEW()
    dm.dmSize = ctypes.sizeof(DEVMODEW)
    user32.EnumDisplaySettingsW(None, -1, ctypes.byref(dm))  # ENUM_CURRENT_SETTINGS
    return dm.dmPelsWidth, dm.dmPelsHeight, dm.dmBitsPerPel


# ---------------------------------------------------------------- boot + window
def test_boots_to_main_menu(menu_game, artifacts):
    shot = menu_game.shot(artifacts / "main_menu.png")
    assert imgcmp.matches(shot, imgcmp.load(GOLDEN / "main_menu.png"))


def test_window_is_framed_not_fullscreen_popup(menu_game):
    style = menu_game.window_style()
    assert style & WS_CAPTION == WS_CAPTION
    assert not style & WS_POPUP


def test_client_area_is_800x600_logical(menu_game):
    h = menu_game.hwnd()
    rc = wt.RECT()
    user32.GetClientRect(h, ctypes.byref(rc))
    # The harness is system-DPI-aware and sees physical pixels; the game is DPI-unaware and gets bitmap-scaled.
    scale = user32.GetDpiForSystem() / 96.0
    assert (round(rc.right / scale), round(rc.bottom / scale)) == (800, 600)


def test_shim_loaded_with_expected_config(menu_game):
    info = menu_game.shim_info()
    assert info.startswith("simgolf_shim windowed=1 bink_dib=1 skip_intro=1")


def test_shim_log_reports_all_hooks(menu_game):
    log = (GAME_DIR / "simgolf_shim.log").read_text()
    for name in ("ChangeDisplaySettingsA", "GetSystemMetrics", "CreateWindowExA", "SetWindowPos",
                 "ClipCursor", "GetCursorPos", "BinkBufferOpen", "BinkOpen"):
        line = next((l for l in log.splitlines() if f"hook {name} @" in l), None)
        assert line, f"{name} hook missing from shim log"
        assert line.endswith(": ok"), line
    assert "ChangeDisplaySettingsA 800x600 16bpp flags=0x4 -> suppressed" in log
    assert "BinkOpen ./flics/SMSG_introfinal.bik -> skipped" in log


# ---------------------------------------------------------------- Frida instrumentation
def test_frida_reads_game_memory(menu_game):
    sc = menu_game.session.create_script("""
        rpc.exports.probe = () => {
          const m = Process.getModuleByName('golf_clean.exe');
          return { base: m.base.toString(), size: m.size, mz: m.base.readU16(), ep: m.base.add(0xA682F).readU8() };
        };""")
    sc.load()
    try:
        r = sc.exports_sync.probe()
    finally:
        sc.unload()
    assert r["base"] == "0x400000"
    assert r["mz"] == 0x5A4D
    assert r["size"] == 0x44414E  # SizeOfImage of golf_clean.exe


def test_frida_interceptor_fires_on_game_thread(menu_game):
    """The main loop polls GetCursorPos; an Interceptor there must fire. Asserts that it fires, not a rate:
    the loop slows down when the window is not in the foreground (16 calls/2 s observed vs ~90/s focused)."""
    sc = menu_game.session.create_script("""
        let n = 0;
        Interceptor.attach(Module.getGlobalExportByName('GetCursorPos'), { onEnter() { n++; } });
        rpc.exports.count = () => n;""")
    sc.load()
    try:
        t0, n = time.time(), 0
        while time.time() - t0 < 10 and n < 5:
            time.sleep(0.25)
            n = sc.exports_sync.count()
    finally:
        sc.unload()
    assert n >= 5


def test_virtual_cursor_is_what_the_game_sees(menu_game):
    sc = menu_game.session.create_script("""
        const u = Process.getModuleByName('user32.dll');
        const gcp = new NativeFunction(u.getExportByName('GetCursorPos'), 'int', ['pointer'], 'stdcall');
        const s2c = new NativeFunction(u.getExportByName('ScreenToClient'), 'int', ['pointer', 'pointer'], 'stdcall');
        rpc.exports.cursor = (hwnd) => { const p = Memory.alloc(8); gcp(p); s2c(ptr(hwnd), p); return [p.readS32(), p.add(4).readS32()]; };""")
    sc.load()
    try:
        menu_game.move(123, 456)
        got = sc.exports_sync.cursor(menu_game.hwnd())
    finally:
        sc.unload()
        menu_game.harness.clear_cursor()
    assert got == [123, 456]
