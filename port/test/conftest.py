"""Fixtures of the portsim harness (phase 2.6). Markers: `static` (pure Python, no DLL), `port` (needs portsim_c.dll,
built here when a source is newer, and the local corpora under port/corpus/, gitignored: tests skip without them).

    ~/bin/py -3.12 -m pytest port/test              # everything (builds first)
    ~/bin/py -3.12 -m pytest port/test -m static    # no build
"""
from __future__ import annotations

import os
import pathlib
import sys

import pytest

ROOT = pathlib.Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))


def pytest_addoption(parser):
    parser.addoption("--backend", choices=["x64", "win32"], default=None,
                     help="portsim backend for the `portlib` fixture (default $SG_BACKEND or x64); see port/test/README.md")


def pytest_configure(config):
    # G30: the app's default profile is `play` (gameplay fixes); app tests compare against the original's recordings, so they run
    # the app under `fixed` unless a test passes --profile (or the caller set SG_APP_PROFILE)
    os.environ.setdefault("SG_APP_PROFILE", "fixed")
    # P5B: the app's window / input defaults (edge guard, +/- zoom keys, close prompt, resizable window, smooth scaling) are off
    # for the app tests, which compare nearest-pixel frames, drive an unfocused window and close it with WM_CLOSE
    os.environ.setdefault("SG_APP_P5B", "off")
    # P6B: the loading overlay (a box drawn into the window while a world load runs) is off for the app tests too
    os.environ.setdefault("SG_APP_P6B", "off")
    # P8B / G32: --wide (the wide course view, its world layer and HUD compositor) is the app's default; off for the app tests
    os.environ.setdefault("SG_APP_P8B", "off")
    # P9A: the edge band fill (--wide-fill edge) is off for the app tests too (it acts only with --wide, which SG_APP_P8B=off
    # already turns off)
    os.environ.setdefault("SG_APP_P9", "off")
    # P9C: the text redraw at the window's resolution (--text-res native) is off for the app tests: they compare the client
    # image with the frame's own pixels at sizes other than 800 x 600 (at 800 x 600 native and frame are the same picture)
    os.environ.setdefault("SG_APP_P9C", "off")
    # P9E: the layer-compositor fixes (popups at their panel's place, ...) are off for the app tests too (they act only with
    # --wide, which SG_APP_P8B=off already turns off)
    os.environ.setdefault("SG_APP_P9E", "off")
    # P10: the fourth play-test's app fixes are off for the app tests (no FPS overlay in the window pixels; P10C fix 3 makes the
    # live game's `snapshots` folder, so the bulletin board would draw photos the original's recordings, made without it, do not have)
    os.environ.setdefault("SG_APP_P10", "off")
    if config.getoption("--backend"):
        os.environ["SG_BACKEND"] = config.getoption("--backend")
    for m in ("static", "port", "x87"):
        config.addinivalue_line("markers", f"{m}: see port/test/conftest.py")


@pytest.fixture(scope="session")
def portlib():
    from port.test import lib
    return lib.load()


@pytest.fixture(scope="session")
def lib32():
    """The Win32 backend next to whatever `portlib` is (phase 3.R); skips with the reason when it cannot be had."""
    from port.test import lib32 as m
    try:
        return m.load32()
    except m.RunnerUnavailable as e:
        pytest.skip(f"Win32 backend unavailable: {e}")


@pytest.fixture(scope="session")
def lib64():
    """The x64 in-process library whatever SG_BACKEND says."""
    from port.test import lib
    if lib.backend() != "win32":
        return lib.load()
    if lib.B.stale("x64"):
        lib.B.build("x64", quiet=True)
    return lib.Lib(lib.B.dll_path("x64"))


@pytest.fixture(scope="session")
def vrunner(portlib):
    from port.test.vectors import VectorRunner
    return VectorRunner(portlib)


@pytest.fixture(scope="session")
def srunner(portlib):
    from port.test.segments import SegmentRunner
    return SegmentRunner(portlib)


@pytest.fixture(autouse=True)
def _negctl_reset(request):
    """Every test starts and ends with the correct bodies selected and every defect switch on (phase 3.0)."""
    yield
    for name in ("portlib", "lib32", "lib64"):
        if name in request.fixturenames:
            request.getfixturevalue(name).negctl_reset()
            request.getfixturevalue(name).defect_reset()
