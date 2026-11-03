"""P4A (decision G30): gameplay fix switches for money / reports and golfers / stories. Every switch defaults to 1 (the
original); 0 is the fix. Cheap state tests for the switches a single registered call reaches; the others (D17, D19, D23,
D24, D26) are judged by the build and the segments gate at default 1. Notes: log/roadmap/P4/P4A_notes.md."""
from __future__ import annotations

import pytest

from port.test.test_golfers import S, VL, needs

pytestmark = pytest.mark.port

SWITCHES = [
    "loadGolferRoster.namedStoryStride",    # D17
    "loadGolferRoster.skillsNextRecord",    # D19
    "mainLoop.cashLowWordOnly",             # D23
    "showFinancialReport.sumWriteBack",     # D24
    "emitGolferThought.tileMoodWrap",       # D26
    "resetGolferState.singleHoleRecord",    # D44
    "computeStoryReply.clampLowerBound",    # D48b
]

HOLE = 0x208


@pytest.fixture(autouse=True)
def _reset(portlib):
    portlib.defect_reset()
    yield
    portlib.defect_reset()


def test_switches_registered_default_original(portlib):
    d = portlib.defects()
    for name in SWITCHES:
        assert name in d, name
        assert d[name]["on"] == 1, name


def _reset_golfer_state(portlib, switch):
    portlib.defect("resetGolferState.singleHoleRecord", switch)
    with portlib.state() as st:
        st.set_data_base(VL.DATA_VA)
        s = S(portlib, st)
        s.w("g_holesPlusOne", 0, "i", 7)              # the record the original works on: par 0, so it is skipped
        for n, length in ((3, 0x272), (5, 0x32)):
            s.w("g_holes", n * HOLE, "b", 1)
            s.w("g_holes", n * HOLE + 4, "h", length)
        st.call("resetGolferState", [])
        return s.r("g_holes", 3 * HOLE, "b"), s.r("g_holes", 5 * HOLE, "b"), s.r("g_holes", 7 * HOLE, "b")


def test_reset_golfer_state_switch(portlib):
    needs(portlib, "updateMembership")
    assert _reset_golfer_state(portlib, 1) == (1, 1, 0), "original: only g_holesPlusOne's record is visited"
    assert _reset_golfer_state(portlib, 0) == (6, 2, 0), "fixed: every hole record 1..18 gets its par"


def _story_reply(portlib, switch):
    portlib.defect("computeStoryReply.clampLowerBound", switch)
    with portlib.state() as st:
        st.set_data_base(VL.DATA_VA)
        s = S(portlib, st)
        g0, g1 = 0, 0x100                             # golfer records 0 and 1 (0x100 bytes each)
        s.w("g_golfers", g0 + 0xa2, "h", 1)           # own partner = golfer 1
        s.w("g_golfers", g1 + 0x21, "b", 3)           # partner on hole 3, counter byte 0
        s.w("g_golfers", g1 + 0xa4, "h", 5)           # mood 5
        s.w("g_golfers", g1 + 0xb4, "h", 40)          # story counter 40
        st.call("computeStoryReply", [0, 100, 3, 0])
        return s.r("g_storyReplyChapter", 0, "i")


def test_compute_story_reply_switch(portlib):
    on, off = _story_reply(portlib, 1), _story_reply(portlib, 0)
    assert on == (40 - 1) // 2, "original: clamp(5, 40, 99) = 40"
    assert off == (5 - 1) // 2, "fixed: clamp(5, 0, 99) = 5"
