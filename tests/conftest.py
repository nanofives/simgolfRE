"""SimGolf RE test suite -- shared fixtures.

Layers (select with -m):
  static   pure file checks, no process launched (seconds)
  runtime  launches the game windowed through the shim + Frida harness (serial: the game is single-instance)
  ghidra   runs Ghidra headless against the master project (slow, ~1 min)
  slow     repeated-boot reliability runs

    py -3.12 -m pytest tests -m static
    py -3.12 -m pytest tests -m "runtime and not slow"
    py -3.12 -m pytest tests            # everything
"""
from __future__ import annotations

import json
import pathlib
import sys
import time

import pytest

ROOT = pathlib.Path(__file__).resolve().parents[1]
GAME_DIR = ROOT / "original"
GOLDEN = pathlib.Path(__file__).parent / "golden"
ARTIFACTS = ROOT / "log" / "test_artifacts"
sys.path.insert(0, str(ROOT / "re" / "tools"))

from game import Game  # noqa: E402
import imgcmp  # noqa: E402

# Screen coordinates in game space (800x600), measured on the v1.03 main menu.
MAIN_MENU_SANDBOX = (130, 445)
LOCATION_MONTEREY = (270, 32)   # the round icon; the text label is not clickable


def pytest_configure(config):
    for m in ("static", "runtime", "ghidra", "slow"):
        config.addinivalue_line("markers", f"{m}: see tests/conftest.py")


@pytest.fixture(scope="session")
def anchors():
    return json.loads((ROOT / "re" / "anchors.json").read_text())


@pytest.fixture(scope="session")
def artifacts():
    ARTIFACTS.mkdir(parents=True, exist_ok=True)
    return ARTIFACTS


def _require_no_running_instance():
    others = Game.running_instances()
    if others:
        pytest.fail(f"SimGolf already running (PIDs {others}). The game is single-instance; "
                    "close it (or wait for the other session) before running runtime tests.")


def boot_to_menu(timeout: float = 30) -> Game:
    """Fresh boot with the intro skipped; returns a Game sitting on the main menu."""
    _require_no_running_instance()
    g = Game(env={"SIMGOLF_SKIP_INTRO": "1"}).start()
    try:
        g.wait_window(timeout=20)
        ref = imgcmp.load(GOLDEN / "main_menu.png")
        imgcmp.wait_for(g, ref, timeout=timeout)
    except Exception:
        g.kill()
        raise
    return g


@pytest.fixture(scope="module")
def menu_game():
    """One game instance shared by a module's read-only tests (boot is the expensive part).
    Module scope, not session: the game is single-instance, so it must be gone before the next
    module's fresh boots."""
    g = boot_to_menu()
    yield g
    g.kill()


@pytest.fixture
def fresh_game():
    """A dedicated instance for tests that change game state."""
    g = boot_to_menu()
    yield g
    g.kill()
