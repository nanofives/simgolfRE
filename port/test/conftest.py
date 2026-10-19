"""Fixtures of the portsim harness (phase 2.6). Markers: `static` (pure Python, no DLL), `port` (needs portsim_c.dll,
built here when a source is newer, and the local corpora under port/corpus/, gitignored: tests skip without them).

    ~/bin/py -3.12 -m pytest port/test              # everything (builds first)
    ~/bin/py -3.12 -m pytest port/test -m static    # no build
"""
from __future__ import annotations

import pathlib
import sys

import pytest

ROOT = pathlib.Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))


def pytest_configure(config):
    for m in ("static", "port"):
        config.addinivalue_line("markers", f"{m}: see port/test/conftest.py")


@pytest.fixture(scope="session")
def portlib():
    from port.test import lib
    return lib.load()


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
    """Every test starts and ends with the correct bodies selected."""
    yield
    if "portlib" in request.fixturenames:
        request.getfixturevalue("portlib").negctl_reset()
