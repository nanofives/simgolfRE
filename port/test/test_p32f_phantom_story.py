"""P32F (play-test 14 item 12): initGolferState.phantomStory.

initGolferState (after every tournament, through resetGolferState) seeds two golfers per hole with updateMembership, which zeroes
the whole record, and never calls pairGolfersWithStory: the original leaves story id 0 (a real story) with no story-owner flag
(golfer +0x10 bit 20), so the info panel offers story 0 and a Next Chapter button mainLoop ignores. At 0 the seeded golfers get
story id -1. A pair that starts afterwards (pairGolfersWithStory) can draw story 0 again: at 1 the seeded golfers' story 0 blocks it
(pairGolfersWithStory's duplicate check over golfers on the course).
"""
from __future__ import annotations

import pathlib
import struct
import sys

import pytest

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import vector_lib as VL  # noqa: E402

pytestmark = pytest.mark.port
GOLFER, HOLE, TYPE_STATE, STORY_DEF = 0x100, 0x208, 0x2c, 50
SWITCH = "initGolferState.phantomStory"
OWNER = 1 << 20
HOLES = (1, 2, 3)


def off(lib, name):
    return next(o for n, o, s in lib.objects() if n == name)


def _seed(lib, st):
    """Three holes with a par, every golfer type a member, only story 0 defined (named story index 99, undefined)."""
    st.set_data_base(VL.DATA_VA)
    holes, ts, sd = off(lib, "g_holes"), off(lib, "g_golferTypeState"), off(lib, "g_storyDefs")
    for h in HOLES:
        st.write(holes + h * HOLE, b"\x04")
    for t in range(76):
        st.write(ts + t * TYPE_STATE + 2, b"\x01")
    st.write(sd, b"S")
    st.write(off(lib, "g_namedStoryIndex"), struct.pack("<i", 99))


def _golfer(st, gf, g):
    r = st.read(gf + g * GOLFER, GOLFER)
    return {"hole": struct.unpack_from("<b", r, 0x21)[0], "story": struct.unpack_from("<h", r, 0xb0)[0],
            "flags": struct.unpack_from("<I", r, 0x10)[0], "partner": struct.unpack_from("<h", r, 0xa2)[0]}


@pytest.mark.parametrize("on", [1, 0])
def test_seeded_golfers_story(portlib, on):
    gf = off(portlib, "g_golfers")
    try:
        portlib.defect(SWITCH, on)
        with portlib.state() as st:
            _seed(portlib, st)
            st.call("initGolferState", [0])
            seeded = [(g, _golfer(st, gf, g)) for g in range(0x98) if _golfer(st, gf, g)["hole"] != 0]
            # a pair starting afterwards (mainLoop calls pairGolfersWithStory(g, g ^ 1) at the first tee)
            st.call("pairGolfersWithStory", [100, 101])
            p1, p2 = _golfer(st, gf, 100), _golfer(st, gf, 101)
    finally:
        portlib.defect(SWITCH, 1)
    assert sorted(r["hole"] for _, r in seeded) == sorted(h for h in HOLES for _ in range(2))
    for g, r in seeded:
        assert r["partner"] == g ^ 1
        assert r["flags"] & OWNER == 0
        assert r["story"] == (0 if on else -1)
    # story 0 is the only one defined: blocked by the seeded golfers at 1 (the original), drawn at 0
    assert p1["story"] == p2["story"] == (-1 if on else 0)
    assert bool(p1["flags"] & OWNER) == (on == 0)


def test_switch_is_registered_original_by_default(portlib):
    d = portlib.defects()
    assert SWITCH in d
    with portlib.state() as st:
        _seed(portlib, st)
        st.call("initGolferState", [0])
        gf = off(portlib, "g_golfers")
        assert all(_golfer(st, gf, g)["story"] == 0 for g in range(6))
