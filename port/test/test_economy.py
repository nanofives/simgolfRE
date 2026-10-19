"""Phase 3.2 economy and membership: negative controls of every ported function (port/src/sim/economy.cpp).
Vectors are judged by test_vectors.py (every registered function); this file proves the vectors can see a wrong body."""
from __future__ import annotations

import pathlib
import sys

import pytest

from port.test import vectors as V

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import vector_lib as VL  # noqa: E402

pytestmark = pytest.mark.port
need_corpus = pytest.mark.skipif(not VL.VEC_DIR.exists(), reason="vector corpus not present (port/corpus/vectors, local)")

# (function, mode, what): wrong bodies the vectors must see (RED)
NEGCTL_RED = [
    ("computeRatingTrend", 1, "sel 2 uses slot 6"), ("computeRatingTrend", 2, "weights start at 0"),
    ("appendDate", 1, "year + 2000"), ("appendDate", 2, "month % 7"),
    ("appendNumber", 1, "no zero padding"),
    ("appendRatingLabel", 1, "q > 3 selects label 3"),
    ("appendToBuffer45b8b0", 1, "first free slot index > 0x1f"),
    ("clearCost", 1, "type 4 costs 13"), ("clearCost", 2, "collar 13 uses cost * 2 / 2"),
    ("rateLot", 1, "east ring column x + 1"), ("rateLot", 2, "flag bits add 50"),
    ("updateRollingStats", 2, "decay shift 5"),
    ("updateMembership", 1, "ties counted from 0"), ("updateMembership", 2, "998 attempts"),
]
# wrong bodies the vectors cannot see (recorded blind spots), filled from the first run
NEGCTL_BLIND = [("updateRollingStats", 1, "no 0x400 grace: 3 vectors; own state test below covers it")]
PHASE32 = sorted({f for f, _, _ in NEGCTL_RED})


@need_corpus
@pytest.mark.parametrize("function,mode,what", NEGCTL_RED)
def test_negctl_reads_red(portlib, vrunner, function, mode, what):
    assert portlib.negctl(function, mode) == 0
    r = vrunner.run_function(function)
    assert r.verdict == "RED" and r.failed > 0, f"{function} mode {mode} ({what}) read {r.summary()}"
    portlib.negctl_reset()
    assert vrunner.run_function(function).verdict == "GREEN"


@need_corpus
@pytest.mark.parametrize("function,mode,what", NEGCTL_BLIND)
def test_negctl_blind_spots_are_known(portlib, vrunner, function, mode, what):
    portlib.negctl(function, mode)
    assert vrunner.run_function(function).verdict == "GREEN", f"{function} mode {mode} ({what}) is no longer blind"


def test_every_phase32_function_has_a_negctl(portlib):
    funcs = portlib.functions()
    for f in PHASE32:
        assert f in funcs, f
        assert portlib.negctl(f, 0) == 0, f


@pytest.mark.parametrize("mode", [0, 1])
def test_rolling_stats_free_grace(portlib, mode):
    """updateRollingStats frees a landed ball record only when the date is past its stamp + 0x400 (state test: the 3
    corpus vectors never sit on the boundary). Mode 1 (no grace) frees at date == stamp + 0x400 and is caught here."""
    import struct
    objs = {n: o for n, o, s in portlib.objects()}
    rec, date = objs["g_ballRecords"], objs["g_date"]
    portlib.negctl("updateRollingStats", mode)
    freed = []
    for d in (0x400, 0x401):
        with portlib.state() as st:
            st.write(rec, bytes(0x2400))
            for i in range(256):                       # all free except record 3: landed (height 0), stamp 0
                st.write(rec + i * 0x24 + 8, struct.pack("<i", -1))
            st.write(rec + 3 * 0x24 + 8, struct.pack("<i", 100))
            st.write(date, struct.pack("<i", d))
            st.call("updateRollingStats", [])
            freed.append(struct.unpack("<i", st.read(rec + 3 * 0x24 + 8, 4))[0] == -1)
    assert freed == ([False, True] if mode == 0 else [True, True])


def test_update_membership_decline_arm(portlib):
    """Every type banned (+0x29 = 0xff): 999 draws are refused, the call returns -1, the slot counter steps back to its
    start value and the decline text is copied (the vectors never reach this arm: U-0844)."""
    import struct
    objs = {n: o for n, o, s in portlib.objects()}
    ts, ctr, txt = objs["g_golferTypeState"], objs["g_nextGolferSlot"], objs["g_textBuffer"]
    with portlib.state() as st:
        for t in range(84):
            st.write(ts + t * 0x2c + 0x29, b"\xff")
        st.write(ctr, struct.pack("<i", 5))
        r = st.call("updateMembership", [0])
        assert r.eax == 0xFFFFFFFF
        assert struct.unpack("<i", st.read(ctr, 4))[0] == 5
        assert st.read(txt, 1) == b"\0", "no pool text without the exe constants: the copy writes the (zero) pool bytes"
