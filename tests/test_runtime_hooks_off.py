"""SIMGOLF_HOOKS_OFF parsing in the shim (its own module: it boots its own game, so no other module's shared instance may
be up)."""
import pytest

from conftest import GAME_DIR

pytestmark = pytest.mark.runtime


def test_hooks_off_list_longer_than_2kb():
    """SIMGOLF_HOOKS_OFF of any length switches its hooks off. Until 2026-10-11 the shim read it into a 2,048-byte
    buffer and a longer list left every hook ON (c3ea), which voided the callers-off passes of rounds 22, 26-28."""
    from conftest import _require_no_running_instance
    from game import Game
    _require_no_running_instance()
    filler = ",".join(f"0x{0x00500000 + 16 * i:08x}" for i in range(400))      # ~4.4 KB, no hook at these addresses
    g = Game(env={"SIMGOLF_SKIP_INTRO": "1", "SIMGOLF_HOOKS_OFF": filler + ",0x00484940,0x004481b0"}).start()
    try:
        g.wait_window(timeout=20)
        log = (GAME_DIR / "simgolf_shim.log").read_text()
    finally:
        g.kill()
    assert len(filler) > 4000
    assert "re-hook Snd_play golf_clean.exe+00484940: OFF" in log
    assert "re-hook playSound golf_clean.exe+004481b0: OFF" in log
    assert "re-hook Snd_reopen golf_clean.exe+00484d60: ON" in log
