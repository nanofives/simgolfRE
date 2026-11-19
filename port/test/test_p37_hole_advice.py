"""P37A (re/roadmap/P37_HOLE_ADVISOR.md): the hole advisor's analysis (port/src/sim/holeadvice.cpp analyze_hole) against the
port's rateCourseHoles (events_news.inc rate_course_holes, A/B-verified against the original: test_events.py).

On random courses (open and closed holes, random histograms, plays, mood sums, comment counts, tees, cups and bend bits, every
difficulty) analyze_hole, run BEFORE the rating pass on the same image, must predict what rateCourseHoles then writes: the
too-hard / too-easy bits (4 / 8) and the notable bits (0x100 / 0x200 / 0x400) of every hole's flags, the sameness counter
+0x1fc, and the course totals g_skillRatingX100 (the sum of every hole's skill) and g_funRating (the sum of the fun terms).
Takings stay at or under 200 and every notice bit is set, so the pass posts no message. The analysis and the advisor's lines
leave the GameState unchanged. Negative control: a test-side mutation of the expected band (too hard one stroke later) reads RED.
"""
from __future__ import annotations

import random
import struct

import pytest

from port.test import lib as L
from port.test import test_commands as TC

pytestmark = [pytest.mark.port,
              pytest.mark.skipif(not L.exe_path().exists(), reason="the user's golf_clean.exe is not present")]
REC = 0x208
OUT_VA = 0x30000000


def _course(lib, rng, difficulty):
    img = bytearray(TC.base_image(lib))
    TC.put32(lib, img, "g_difficulty", difficulty)
    TC.put32(lib, img, "g_holeNoticeMask", 0xFFFFFFFF)
    TC.put32(lib, img, "g_forcedEventBits", 0)
    for h in range(0, 20):
        rec = bytearray(REC)
        if 1 <= h <= 18 and rng.random() < 0.85:
            par = rng.choice([3, 4, 4, 5])
            rec[0] = par
            struct.pack_into("<h", rec, 4, rng.randrange(100, 600))
            struct.pack_into("<4i", rec, 8, rng.randrange(0, 200), rng.randrange(0, 200), 0, 0)
            struct.pack_into("<2i", rec, 0x18, rng.randrange(0, 200), rng.randrange(0, 200))
            plays = rng.choice([0, 3, 8, 9, 15, 40])
            struct.pack_into("<2i", rec, 0x20, plays, rng.randrange(0, 80))
            dense = rng.random()
            for col in range(8):
                for row in range(1, 10):
                    if rng.random() < dense * (1.0 if abs(row - par) <= 2 else 0.3):
                        struct.pack_into("<h", rec, 0x28 + 2 * (col * 11 + row), rng.randrange(0, 12))
            for k in range(64):
                if rng.random() < 0.15:
                    struct.pack_into("<h", rec, 0xd8 + 2 * k, rng.randrange(0, 20))
            struct.pack_into("<h", rec, 0x158, rng.randrange(-80, 80))
            struct.pack_into("<i", rec, 0x1f4, rng.randrange(-50, 201))
            struct.pack_into("<I", rec, 0x200, rng.choice([0, 0x20, 0x40, 0x60]) | rng.choice([0, 0x1000, 0x2000]))
        TC.put(lib, img, "g_holes", bytes(rec), REC * h)
    return bytes(img)


def _analyze(st, buf, hole):
    st.call("holeAdvice.analyze", [hole, OUT_VA])
    return list(buf)


def _run(lib, img):
    import ctypes as C
    with lib.state() as st:
        if st.lib.has("audio_exe.stubRequests"):
            st.call("audio_exe.stubRequests", [1])
        st.set_data_base(TC.DATA_VA)
        st.load_image(img)
        buf = (C.c_int32 * 32)()
        st.map(OUT_VA, buf)
        got = {h: _analyze(st, buf, h) for h in range(1, 19)}
        assert st.image() == img, "analyze_hole wrote the GameState"
        st.call("rateCourseHoles", [])
        out = st.image()
    return got, out


def _expect(lib, out, h):
    off = TC.objs(lib)["g_holes"][0] + REC * h
    flags = struct.unpack_from("<I", out, off + 0x200)[0]
    same = struct.unpack_from("<i", out, off + 0x1fc)[0]
    return flags, same


SEEN = {"hard": 0, "easy": 0, "notable": 0, "same": 0}   # the cases the rating pass produced (the oracle is not vacuous)


def _check(lib, img, mutate_band=False):
    got, out = _run(lib, img)
    o = TC.objs(lib)
    skill_sum = fun_sum = 0
    bad = []
    for h in range(1, 19):
        w = got[h]
        is_open, too_hard, too_easy, notable, skill, fun, sameness = w[0], w[5], w[6], w[13], w[15], w[16], w[25]
        if not is_open:
            continue
        flags, same = _expect(lib, out, h)
        if mutate_band:
            n, s, par = w[2], w[3], w[1]
            d = struct.unpack_from("<i", img, o["g_difficulty"][0])[0]
            too_hard = int(n >= 10 and ((6 - d) * n) // 3 + par * n + n // 2 < s)   # half a stroke per play later
        SEEN["hard"] += bool(flags & 4)
        SEEN["easy"] += bool(flags & 8)
        SEEN["notable"] += bool(flags & 0x700)
        SEEN["same"] += same >= 2
        if bool(flags & 4) != bool(too_hard) or bool(flags & 8) != bool(too_easy):
            bad.append(f"hole {h} band: flags {flags:#x} analysis hard {too_hard} easy {too_easy}")
        if (flags & 0x700) != notable:
            bad.append(f"hole {h} notable: flags {flags & 0x700:#x} analysis {notable:#x}")
        if h > 1 and same != sameness:
            bad.append(f"hole {h} sameness: pass {same} analysis {sameness}")
        skill_sum += skill
        fun_sum += fun
    if struct.unpack_from("<i", out, o["g_skillRatingX100"][0])[0] != skill_sum:
        bad.append("skill total")
    if struct.unpack_from("<i", out, o["g_funRating"][0])[0] != fun_sum:
        bad.append("fun total")
    return bad


@pytest.mark.parametrize("difficulty", [0, 1, 2, 3])
def test_analysis_predicts_the_rating_pass(portlib, difficulty):
    rng = random.Random(f"P37A{difficulty}")
    bad = []
    for k in SEEN:
        SEEN[k] = 0
    for _ in range(40):
        bad += _check(portlib, _course(portlib, rng, difficulty))
    assert not bad, bad[:10]
    assert all(v > 0 for v in SEEN.values()), SEEN


def test_negative_control_band_reads_red(portlib):
    rng = random.Random("P37A-neg")
    hits = sum(1 for _ in range(40) if _check(portlib, _course(portlib, rng, 1), mutate_band=True))
    assert hits > 0


def test_lines_read_only_and_complete(portlib):
    import ctypes as C
    rng = random.Random("P37A-lines")
    img = _course(portlib, rng, 2)
    with portlib.state() as st:
        st.set_data_base(TC.DATA_VA)
        st.load_image(img)
        buf = C.create_string_buffer(8192)
        st.map(OUT_VA, buf)
        texts = []
        for h in range(1, 19):
            n = st.call("holeAdvice.lines", [h, OUT_VA, 8192]).eax
            texts.append(buf.raw[:n].decode("latin-1"))
        assert st.image() == img, "the advisor's lines wrote the GameState"
    opened = [t for t in texts if t]
    assert opened, "no open hole in the course"
    for t in opened:
        assert "Best 100" in t and "Top 18" in t and "Skills:" in t and "Money:" in t, t


def test_course_overview(portlib):
    """P37B: the overview has the three summary lines and one row per open hole, ordered by progress to the next award
    (blocked and fully won holes last), and leaves the GameState unchanged"""
    import ctypes as C
    rng = random.Random("P37B")
    for difficulty in (0, 2):
        img = _course(portlib, rng, difficulty)
        with portlib.state() as st:
            st.set_data_base(TC.DATA_VA)
            st.load_image(img)
            buf = C.create_string_buffer(16384)
            st.map(OUT_VA, buf)
            n = st.call("holeAdvice.course", [OUT_VA, 16384]).eax
            lines = buf.raw[:n].decode("latin-1").split("\n")[:-1]
            assert st.image() == img, "the overview wrote the GameState"
        off = TC.objs(portlib)["g_holes"][0]
        open_holes = [h for h in range(1, 19) if img[off + REC * h] != 0]
        rows = lines[3:]
        assert len(rows) == len(open_holes), (len(rows), open_holes)
        assert sorted(int(r.split("\t")[0].split()[-1]) for r in rows) == open_holes
        pct = [int(r.split("\t")[1].split()[-1].rstrip("%")) for r in rows if r.split("\t")[1].endswith("%")]
        ordered = [p for p, r in zip(pct, rows) if not r.split("\t")[2].startswith("too ")]
        assert ordered == sorted(ordered, reverse=True), rows
