"""re/tools/trace_tool.py record / replay end to end on the original game (phase 2.3, 2026-10-18).

Boots instances\\p23 three times on sandbox_basic (about 1 minute on LIVING-PC, marked `slow`): two RECORD runs of the
pinned schedule must give byte-identical, valid traces whose checkpoints equal the pinned reference, and a REPLAY driven
only by the trace (recorded clock values and deliveries; scenario_det.js's virtual clock never answers the main thread)
must write a byte-identical trace, with no call-site mismatch, every delivery applied and the checkpoints equal to the
reference. The instance must exist (re/tools/instances.py create p23 + winmm.dll of the pinned references' build).
"""
import json
import subprocess
import sys

import pytest

from conftest import ROOT

pytestmark = [pytest.mark.runtime, pytest.mark.slow]

TOOL = [sys.executable, str(ROOT / "re" / "tools" / "trace_tool.py")]
OUT = ROOT / "log" / "p2" / "p23" / "test"


def run(*args, timeout=1200):
    r = subprocess.run(TOOL + list(args), capture_output=True, text=True, timeout=timeout, cwd=str(ROOT))
    return r.returncode, r.stdout + r.stderr


@pytest.fixture(scope="module")
def records():
    if not (ROOT / "instances" / "p23" / "golf_clean.exe").exists():
        pytest.skip("instances\\p23 missing (re/tools/instances.py create p23)")
    out = []
    for label in ("t_rec_a", "t_rec_b"):
        p = OUT / f"sandbox_basic.{label}.trace"
        rc, log = run("record", "--scenario", "sandbox_basic", "--label", label, "--out", str(p))
        assert rc == 0, log[-2000:]
        out.append(p)
    return out


def test_two_records_are_byte_identical_and_match_the_reference(records):
    a, b = records
    assert a.read_bytes() == b.read_bytes()
    for p in records:
        info = json.loads((p.parent / (p.name + ".json")).read_text())
        assert info["validate"] == "OK"
        assert info["reference"].startswith("SAME"), info["reference"]
        assert info["summary"]["frames"] > 400 and info["summary"]["date_step_per_frame"]["median"] == 1


def test_replay_from_the_trace_alone(records):
    p = OUT / "sandbox_basic.t_rep.trace"
    rc, log = run("replay", "--scenario", "sandbox_basic", "--label", "t_rep", "--trace", str(records[0]), "--out", str(p))
    assert rc == 0, log[-2000:]
    info = json.loads((p.parent / (p.name + ".json")).read_text())
    assert info["byte_identical"], info["same_as_source"]
    rep = info["stats"]["replay"]
    assert rep["mismatches"] == 0 and rep["overrun"] == 0 and rep["delivered"] == rep["planned"]
    assert info["reference"].startswith("SAME"), info["reference"]
