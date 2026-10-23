"""LiveTerrain: the port's exe-side Terrain calls and the Terrain.dll state they build, against the original on pinned scenarios
(phase 5 step 3, re/PHASE5_TERRAIN_PLAN.md section 14; harness port/test/terrainlive.py).

    ~/bin/py -3.12 -m pytest port/test/test_terrain_live.py -q
"""
from __future__ import annotations

import pytest

from port.test import terrainlive as TL

pytestmark = pytest.mark.port

SCENARIOS = TL.pinned()
_RUNS: dict = {}
# deliberately wrong bodies the pinned runs must see: 22 the exported resetTerrain does nothing (the course switch, a call-log and
# state divergence), 14 rand()'s increment (the drawn variations in the log), 12 elevateCorner's centre min for max (state only)
NEGCTL = {22: "calls", 14: "calls", 12: "state"}
NEGCTL_SCENARIO = "sandbox_basic"


@pytest.fixture(scope="module")
def lib():
    from port.test import lib as L
    lb = L.load()
    TL.bind(lb.dll)
    return lb


def _run(lib, scenario):
    if scenario not in _RUNS:
        _RUNS[scenario] = TL.run(lib, scenario)
    return _RUNS[scenario]


def _need(scenario):
    if scenario == "none":
        pytest.skip("no pinned terrain recording (re/tools/terrain_calls.py --scenario <name>; python -m port.test.terraincorpus export)")


@pytest.mark.parametrize("scenario", SCENARIOS or ["none"])
def test_boot_still_passes(lib, scenario):
    """With the port's own Terrain.dll state attached, the boot gate's sim hashes stay the original's."""
    _need(scenario)
    r = _run(lib, scenario)
    assert r.boot.status in ("PASS", "KNOWN"), (r.boot.status, r.boot.detail)


@pytest.mark.parametrize("scenario", SCENARIOS or ["none"])
def test_calls_equal_the_original(lib, scenario):
    _need(scenario)
    r = _run(lib, scenario)
    assert r.calls_equal, f"{len(r.orig)} / {len(r.port)} calls, first difference {r.first_call_diff}"


def _checkpoints(s):
    root = TL.TC.CORPUS / ("pin_" + s)
    names = TL.TC.snaps(root)
    until = TL.json.loads((root / "index.json").read_text()).get("valid_until")
    return names[:names.index(until) + 1] if until else names


KEYS = [(s, n) for s in SCENARIOS for n in _checkpoints(s)]


@pytest.mark.parametrize("scenario,checkpoint", KEYS or [("none", "none")])
def test_state_equals_the_original(lib, scenario, checkpoint):
    _need(scenario)
    d = _run(lib, scenario).states[checkpoint]
    assert not any(d.values()), d


@pytest.mark.parametrize("mode", sorted(NEGCTL))
def test_negctl(lib, mode):
    if NEGCTL_SCENARIO not in SCENARIOS:
        pytest.skip(f"no pinned recording of {NEGCTL_SCENARIO}")
    lib.dll.sg_ts_negctl(mode)
    try:
        r = TL.run(lib, NEGCTL_SCENARIO)
    finally:
        lib.dll.sg_ts_negctl(0)
    if NEGCTL[mode] == "calls":
        assert not r.calls_equal, f"mode {mode} leaves the call log equal"
    assert any(any(d.values()) for d in r.states.values()) or not r.calls_equal, f"mode {mode} reads GREEN"
