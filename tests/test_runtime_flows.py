"""Runtime flows that need their own fresh game instance (display mode, navigation, boot reliability).

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
from conftest import GAME_DIR, GOLDEN, LOCATION_MONTEREY, MAIN_MENU_SANDBOX, boot_to_menu

pytestmark = pytest.mark.runtime
user32 = ctypes.windll.user32

from test_runtime import _display_mode  # noqa: E402


def test_display_mode_untouched():
    before = _display_mode()
    g = boot_to_menu()
    try:
        assert _display_mode() == before
    finally:
        g.kill()
    assert _display_mode() == before


# ---------------------------------------------------------------- input + in-game
def test_navigate_to_sandbox_ingame(fresh_game, artifacts):
    g = fresh_game
    g.click(*MAIN_MENU_SANDBOX)
    imgcmp.wait_for(g, imgcmp.load(GOLDEN / "location.png"), timeout=10)
    g.click(*LOCATION_MONTEREY)
    # The course is randomised per new game, so only fixed HUD regions are compared: the control dial
    # (bottom-left) and the inside of the course logo disc (top-left). Measured over two Monterey games:
    # same HUD dial 5.7 / logo 9.7, main menu and location screens >= 83. Threshold 30 per region.
    dial = imgcmp.load(GOLDEN / "ingame_hud_dial.png")
    logo = imgcmp.load(GOLDEN / "ingame_hud_logo.png")
    t0 = time.time()
    while time.time() - t0 < 30:
        shot = g.shot()
        if imgcmp.matches(shot.crop((15, 535, 140, 595)), dial, 30) and imgcmp.matches(shot.crop((18, 18, 58, 58)), logo, 30):
            break
        assert g.alive, g.detach_reason
        time.sleep(1)
    else:
        g.shot(artifacts / "ingame_fail.png")
        pytest.fail("in-game HUD never appeared")
    g.shot(artifacts / "ingame.png")
    time.sleep(5)  # OpenGL terrain keeps running
    assert g.alive


@pytest.mark.slow
@pytest.mark.parametrize("attempt", range(5))
def test_boot_reliability(attempt):
    g = boot_to_menu()
    try:
        time.sleep(3)
        assert g.alive
    finally:
        g.kill()


def test_plain_launch_without_frida():
    """The player path (Play SimGolf.bat): no Frida, just the exe + shim. Kill only our PID."""
    import subprocess
    from game import Game, _enum_windows
    others = Game.running_instances()
    if others:
        pytest.fail(f"SimGolf already running (PIDs {others})")
    p = subprocess.Popen([str(GAME_DIR / "golf_clean.exe")], cwd=GAME_DIR)
    try:
        t0 = time.time()
        while time.time() - t0 < 20 and not _enum_windows(p.pid, "JackalClass"):
            assert p.poll() is None, f"exited with {p.returncode}"
            time.sleep(0.2)
        h = _enum_windows(p.pid, "JackalClass")
        assert h, "no window"
        assert user32.GetWindowLongW(h[0], -16) & 0x00C00000 == 0x00C00000  # framed
        time.sleep(3)
        assert p.poll() is None
    finally:
        p.kill()
        p.wait(10)


@pytest.mark.slow
def test_full_intro_with_bink_dib():
    """Intro movie plays to the end through the forced DIB-section Bink buffer, then the menu appears.
    With BINKBUFFERAUTO this path access-violates inside BinkBufferOpen (intermittent)."""
    from game import Game
    if Game.running_instances():
        pytest.fail("SimGolf already running")
    with Game(env={"SIMGOLF_SKIP_INTRO": "0"}) as g:
        g.wait_window()
        imgcmp.wait_for(g, imgcmp.load(GOLDEN / "main_menu.png"), timeout=60)
        log = (GAME_DIR / "simgolf_shim.log").read_text()
        assert "BinkBufferOpen 800x600 flags=0x0 -> 0x2" in log
