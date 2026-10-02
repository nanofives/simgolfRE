"""Screenshot capture must see the game, not whatever is on screen: regression for the false "hangs"
caused by screen-pixel grabs (another window on top) and PrintWindow black frames (2026-10-02)."""
import ctypes
import ctypes.wintypes as wt
import time

import numpy as np
import pytest

import imgcmp
from conftest import GOLDEN

pytestmark = pytest.mark.runtime
user32 = ctypes.windll.user32


def test_no_black_frames_at_menu(menu_game):
    shots = [menu_game.shot() for _ in range(60)]
    assert sum(np.asarray(s).mean() < 5 for s in shots) == 0


def test_capture_ignores_a_window_on_top(menu_game):
    import tkinter as tk
    rc = wt.RECT()
    user32.GetWindowRect(menu_game.hwnd(), ctypes.byref(rc))
    root = tk.Tk()
    root.overrideredirect(True)
    root.attributes("-topmost", True)
    root.geometry(f"{rc.right - rc.left}x{rc.bottom - rc.top}+{rc.left}+{rc.top}")
    root.configure(bg="red")
    root.update()
    time.sleep(0.5)
    root.update()
    try:
        ref = imgcmp.load(GOLDEN / "main_menu.png")
        assert all(imgcmp.matches(menu_game.shot(), ref) for _ in range(10))
    finally:
        root.destroy()
