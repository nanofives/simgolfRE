"""Roadmap S1.4 (log/roadmap/S1/4_notes.md): the boot gate for the scenarios that judge U-1066 part 1 (the tutorial pages) and
U-1051 (input during a world load).

cov_r_tutorial (re/tools/scenario_s1_4.py): showTutorialText pages 1..29, one key per page. The page word is a harness write
(nothing in the exe stores a nonzero page), bit 15 of g_flags a harness read-modify-write after mood5.sve is loaded. Every
record of the port's boot run equals the original's per-frame sim hashes, and the controls that the 60 older scenarios could not
see (showTutorialText 1..3: page not advanced, no startMessage, camera move dropped) read RED here.

The scenario's trace and capture are local (port/corpus/traces, log/p2/p24); a checkout without them skips.
"""
from __future__ import annotations

import json
import struct

import pytest

from port.test import boot as B
from port.test import lib as L

pytestmark = pytest.mark.port

TUT = "cov_r_tutorial"

# (function, mode, what it does wrong)
TUT_NEGCTL = [("showTutorialText", 1, "the page is not advanced after its message"),
              ("showTutorialText", 2, "startMessage is not called"),
              ("showTutorialText", 3, "the camera move is put back when the page returns"),
              ("showTutorialText", 4, "the camera cache reset skipped")]


@pytest.fixture(scope="module")
def portlib():
    return L.load()


def _need(scen):
    if not (B.TRACES / f"{scen}.trace").exists() or B.expected_records(scen)[1] is None:
        pytest.skip(f"no trace or capture for {scen} (port/corpus/traces, log/p2/p24)")


def test_tutorial_boot_passes(portlib):
    _need(TUT)
    r = B.run(TUT, portlib)
    assert r.status == "PASS", f"{TUT}: {r.status} {r.detail} | {r.run_detail[:300]}"
    assert r.records >= 1000, r.records


@pytest.mark.parametrize("fname,mode,what", TUT_NEGCTL)
def test_tutorial_negctl_reads_red(portlib, fname, mode, what):
    _need(TUT)
    portlib.negctl(fname, mode)
    try:
        r = B.run(TUT, portlib)
    finally:
        portlib.negctl_reset()
    assert r.status not in ("PASS", "KNOWN"), f"{fname} mode {mode} ({what}) read GREEN on {TUT}"


# g_tutorialPage / g_flags in the original's frame-head images of the capture (offsets from the start of the .data section)
_DATA_VA = 0x004C1000
_PAGE_OFF, _FLAGS_OFF = 0x00567A18 - _DATA_VA, 0x0059E7B8 - _DATA_VA


def test_tutorial_pages_reached_in_the_original():
    """The capture's checkpoint images show every page: after the key of page n the counter reads n + 1 (pages 1..10 and
    21..28), pages 11 and 29 save and reset it to 0 with bit 15 of g_flags cleared, and pages 12..20 and 30 leave it alone."""
    d = B.CAPTURES / TUT / "c_a"
    if not (d / "p01.fh.bin").exists():
        pytest.skip("no capture of cov_r_tutorial (log/p2/p24)")

    def read(name):
        b = (d / f"{name}.fh.bin").read_bytes()
        return struct.unpack_from("<i", b, _PAGE_OFF)[0], struct.unpack_from("<I", b, _FLAGS_OFF)[0]

    assert read("armed") == (1, 0x41008000)
    for n in list(range(1, 11)) + list(range(21, 29)):
        page, flags = read(f"p{n:02d}")
        assert (page, flags & 0x8000) == (n + 1, 0x8000), (n, page, hex(flags))
    for n in (11, 29):
        assert read(f"p{n:02d}") == (0, 0x41000000), n
    for n in list(range(12, 21)) + [30]:
        page, flags = read(f"p{n:02d}")
        assert (page, flags & 0x8000) == (n, 0x8000), (n, page, hex(flags))


def test_scenarios_are_pinned():
    """Both scenarios are registered and pinned (three identical runs) in the manifest under their checkpoint names."""
    m = json.loads((B.ROOT / "re" / "analysis" / "data" / "scenario_refs.json").read_text())["scenarios"]
    assert "p29" in m[TUT]["checkpoints"] and "esc_load" in m[TUT]["checkpoints"]
    assert m["cov_r_flic_input"]["checkpoints"] == ["menu", "location", "hud", "end"]


# U-1051: a message posted before a world load is dispatched by loadFlic's doAllTasks (+0x54). The port takes it with
# PumpKind::Load (tick.cpp TraceInput, roadmap S1.4). Without it the boot run reads RED at record 1 (g_cachedCameraTileX:
# the original's first frame takes the 'z' and zooms), which tick.frame 8 reproduces.
FLIC = "cov_r_flic_input"


def test_flic_input_boot_passes(portlib):
    _need(FLIC)
    r = B.run(FLIC, portlib)
    assert r.status == "PASS", f"{FLIC}: {r.status} {r.detail} | {r.run_detail[:300]}"


def test_flic_input_negctl_reads_red(portlib):
    """tick.frame 8: the Load pump delivers nothing (the unpatched port's behaviour)."""
    _need(FLIC)
    portlib.negctl("tick.frame", 8)
    try:
        r = B.run(FLIC, portlib)
    finally:
        portlib.negctl_reset()
    assert r.status not in ("PASS", "KNOWN"), "tick.frame mode 8 read GREEN on cov_r_flic_input"
