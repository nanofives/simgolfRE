"""FB10 (play-test feedback 1 item 10, play switch mainLoop.homeLabelBlink, log/roadmap/P41/feedback1/item14_notes.md section 9).

mainLoop's object labels (P4_objectLabel, ML+0x667a..ML+0x6902) draw a house's "<name>'s Home" label (kind 5 with an owner, zoom 4)
only when bits 0x30 of ([F+0x28] + g_date) are clear (ML+0x6768..ML+0x6776): each label shows 16 of every 64 date steps. At 1 (the
original) that is kept; at 0 the label is drawn every frame. Drawing only: the GameState after a frame run is the same.

The frame run: segment cov_k1_animals/00 (the corpus segment whose start image holds an owned house, at zoom 4), run twice with the
callee counts (sg_tick_callee_counts). At 0 the run makes more drawCenteredText2 calls than at 1 (two per house per frame the
blink skipped), every other callee count and the end image are equal.
Run as one file per process: `pytest port/test/test_home_labels.py`.
"""
from __future__ import annotations

import ctypes as C

import pytest

from port.test.test_terrain import need_exe

pytestmark = pytest.mark.port

SWITCH = "mainLoop.homeLabelBlink"
SEGMENT = "cov_k1_animals/00"


@pytest.fixture(autouse=True)
def _reset(portlib):
    portlib.defect_reset()
    yield
    portlib.defect_reset()


def _seg_ok():
    from port.test import segments as S
    return S.SEG_DIR.exists() and S.INSTALL.is_dir()


def test_switch_registered_default_on(portlib):
    d = portlib.defects()
    assert SWITCH in d and d[SWITCH]["on"] == 1 and d[SWITCH]["ref"] == "model_defects#56"


@need_exe
@pytest.mark.skipif(not _seg_ok(), reason="segment corpus or install not present")
def test_frame_run_steady_labels(portlib):
    from port.test import integrator as I
    from port.test import segments as S
    sc, k = SEGMENT.split("/")
    seg = S.load_segment(S.SEG_DIR / sc / k)
    f = portlib.dll.sg_tick_callee_counts
    f.argtypes = [C.c_char_p, C.c_int32, C.c_int32]

    def run(on):
        portlib.defect(SWITCH, on)
        f(C.create_string_buffer(16), 16, 1)
        end = {}
        res = I.run_from(portlib, seg.name, seg, seg.trace, seg.records, None,
                         after=lambda st, r: end.__setitem__("img", st.image()))
        buf = C.create_string_buffer(1 << 20)
        f(buf, 1 << 20, 0)
        counts = {}
        for line in buf.value.decode().splitlines():
            if "=" in line:
                a, v = line.rsplit("=", 1)
                counts[a] = int(v)
        return res, end.get("img"), counts

    r1, img1, c1 = run(True)
    r0, img0, c0 = run(False)
    assert r1.status == "PASS", (r1.status, r1.detail)
    assert r0.frames_run == r1.frames_run, (r0.status, r0.detail)
    assert img0 == img1, "the switch changed the GameState (it is drawing only)"
    keys = set(c0) | set(c1)
    diff = {k_: (c1.get(k_, 0), c0.get(k_, 0)) for k_ in keys if c1.get(k_, 0) != c0.get(k_, 0)}
    print(SEGMENT, "count differences (switch 1, switch 0):", diff, "frames", r1.frames_run)
    assert set(diff) == {"drawCenteredText2"}, diff
    blink, steady = diff["drawCenteredText2"]
    assert steady > blink and (steady - blink) % 2 == 0, diff   # the label's two lines in each frame the blink skipped
