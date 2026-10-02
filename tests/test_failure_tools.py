"""Failure-exploitation tools: SBFL ranking (re/tools/sbfl.py) and invariant oracle (re/tools/invariants.py)."""
import json
import sys

import pytest

from conftest import ROOT

sys.path.insert(0, str(ROOT / "re" / "tools"))
import invariants  # noqa: E402
import sbfl  # noqa: E402

pytestmark = pytest.mark.static


def test_ochiai_ranks_the_function_only_failing_runs_execute():
    common = {("golf_clean.exe", f"{i:08x}") for i in range(10)}
    bad = ("Terrain.dll", "00001d50")
    passing = [set(common), set(common)]
    failing = [common | {bad}, common | {bad}]
    ranked = sbfl.ochiai(passing, failing)
    assert ranked[0][1] == bad and ranked[0][0] == 1.0
    assert all(score < 1.0 for score, e, *_ in ranked[1:])


def _calls():
    calls = []
    for x in range(-1, 11):
        for y in (-1, 0, 5, 9, 10):
            ok = 0 <= x <= 9 and 0 <= y <= 9
            calls.append({"ecx": 0x1000, "edx": 0, "args": [x & 0xFFFFFFFF, y & 0xFFFFFFFF],
                          "live": (0x1000 + 0x3a4 + (y * 10 + x) * 0x248) if ok else 0})
    return calls


def test_invariants_recover_grid_and_stride():
    inv = invariants.infer(_calls())
    assert inv["ret"]["null_iff_outside"] == {"arg0": [0, 9], "arg1": [0, 9]}
    assert inv["ret"]["ecx_relative"]["stride"] == 0x248
    assert inv["ret"]["ecx_relative"]["min_offset"] == 0x3a4


def test_invariants_flag_a_caller_that_breaks_the_contract():
    inv = invariants.infer(_calls())
    wrong = [{"ecx": 0x1000, "edx": 0, "args": [3, 4], "live": 0}]            # in range but NULL
    off_by_one = [{"ecx": 0x1000, "edx": 0, "args": [3, 4], "live": 0x1000 + 0x3a4 + 1}]
    assert invariants.check(inv, wrong)
    assert invariants.check(inv, off_by_one)
    assert not invariants.check(inv, _calls())


def test_live_bundle_invariants_match_the_disassembly():
    p = ROOT / "re" / "replay" / "TerrainDll_tileAt" / "invariants.json"
    if not p.exists():
        pytest.skip("run re/tools/invariants.py TerrainDll_tileAt")
    inv = json.loads(p.read_text())
    assert inv["ret"]["ecx_relative"]["stride"] == 0x248   # imul eax,eax,0x248 at 0x10001da2
    assert inv["ret"]["ecx_relative"]["min_offset"] == 0x3a4  # lea eax,[ecx+eax+0x3a4] at 0x10001dab
