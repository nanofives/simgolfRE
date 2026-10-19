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
