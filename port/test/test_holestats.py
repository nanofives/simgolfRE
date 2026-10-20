"""Phase 3.8 stream 38e: showHoleStatsScreen (port/src/sim/holestats.cpp) and the clock query InputSource::clock (U-0949).

showHoleStatsScreen redraws its page whenever (timeGetTime() >> 10) % (rounds ? 5 : 3) changes and polls input between
clock reads, so the port asks its InputSource for the value of each of the original's clock reads by call site. Evidence:
  * the EMULATOR ORACLE (opt-in SG_EMU_ORACLE=1, port/test/cmdemu.py): the ORIGINAL under Unicorn with the dropped drawing
    stubbed, a SCRIPTED CLOCK per call site (cmdemu run(clock=)) and the same arrival rule for the input batches
    (run(gate=): batch k arrives once the last clock value read is at least arrive[k]); the port gets the same script
    through holestats.setScript. eax-less: the whole data image, the error, the consumed input and the buildGolferComment
    call sequence must be equal (or buildGolferComment runs for real on both sides);
  * GOLDEN digests (port/test/golden_holestats.json) pinned from the port after the oracle was GREEN, and every negative
    control against them;
  * the segments championship/02 and cov_a_keys2/02 (the trace's clock records at the five sites, all served in order).
"""
from __future__ import annotations

import ctypes as C
import json
import os
import pathlib
import random
import struct
import sys

import pytest

from port.test import lib as L
from port.test.test_commands import (DATA_VA, EH_VEC, FAKE_DISPLAY, INPUT_VA, PEER_LOG_VA, SCREEN_DROPPED,
                                     VIEW_TO_LOCAL, base_image, digest, first_diff, fnv1a, objs, put, put32, rand_obj)

ROOT = pathlib.Path(__file__).resolve().parents[2]
GOLDEN = ROOT / "port" / "test" / "golden_holestats.json"

pytestmark = pytest.mark.port
need_exe = pytest.mark.skipif(not L.exe_path().exists(), reason="the user's golf_clean.exe is not present")
emu_oracle = pytest.mark.skipif(os.environ.get("SG_EMU_ORACLE") not in ("1", "update"),
                                reason="opt-in: SG_EMU_ORACLE=1 runs the original under Unicorn (development oracle)")

ENTRY = 0x00453330                 # showHoleStatsScreen (cdecl, the hole)
FRAME_RA = 0x0040FC2F              # mainLoop's frame-head return address: InputSource::clock sites are offsets from it
SITES = {"entry": 0x00453368, "banner": 0x00454239, "after_draw": 0x0045457F, "pass": 0x0045464C, "new_page": 0x00454661}
SITE_OFF = {k: v - FRAME_RA for k, v in SITES.items()}
BUILD_COMMENT = 0x00469B00         # buildGolferComment (cdecl, 4 arguments)
WAIT_TICKS = 0x0045BF80            # dropped by the port (pacing; its pumpInput483c30 dispatches nothing)
DROPPED = sorted(set(SCREEN_DROPPED) | {WAIT_TICKS, 0x00474440, 0x00473F60, 0x00476310, 0x00476700, 0x00476650,
                                        0x00473E60, 0x00480C80})
NO_RET = (0x00483D30, 0x00483C30, 0x00483C70, 0x00483BD0)   # tail-jumping helpers without a `ret` of their own
CMD_VA, ARRIVE_VA, CLOCK_VA = INPUT_VA, INPUT_VA + 0x10000, INPUT_VA + 0x20000
PEER_LOG_CAP = 2048                # buildGolferComment calls: up to five per page build
STOP = (1, 4, 7, 8, 10, 11)


# ------------------------------------------------------------------------------------------------ cases
def _batches(inputs):
    out, cur = [], []
    for c in inputs:
        cur.append(c)
        if c.handler in STOP:
            out.append(cur)
            cur = []
    if cur:
        out.append(cur)
    return out


def _clock_script(rng, t0):
    """per-site value lists: site value k = base + k * step (the steps decide how often the page changes)"""
    script = {}
    for name in SITES:
        n = 1 if name == "entry" else rng.choice([3, 8, 40]) if name != "pass" else rng.choice([20, 80, 300])
        step = rng.choice([1, 12, 12, 100, 700, 1024, 3000])
        base = (t0 + rng.randrange(0, 4000)) & 0xFFFFFFFF
        script[name] = [(base + k * step) & 0xFFFFFFFF for k in range(n)]
    script["entry"] = [t0]
    return script


def _image(lib, rng, hole, real_peer):
    img = bytearray(base_image(lib))
    put32(lib, img, "g_audio5aa774", 0)             # no handler deadline: the handlers' own clock reads are unused
    rand_obj(lib, img, "g_rootInput", rng)
    put32(lib, img, "g_rootInput", rng.choice([0, 0, 1]), 0x1C)
    put32(lib, img, "g_redrawPending", rng.choice([0, 1]))
    put32(lib, img, "gDirtyFlagA", rng.choice([0, 0, 0, 0x41]))
    put32(lib, img, "gDirtyFlagB", rng.choice([0, 0, 0, 0x61]))
    put32(lib, img, "g_ui_569628", rng.choice([0, 1, 2, 3, 5, rng.randrange(-5, 50)]))
    put32(lib, img, "g_msgState53df54", -hole)
    put32(lib, img, "g_graphicsDevice", rng.choice([0, FAKE_DISPLAY]))
    put32(lib, img, "g_commentTone", rng.choice([0, 0x800023E8, 0x80007D08]))   # the comment line's colour test
    goff, gsz = objs(lib)["g_golfers"]
    if real_peer:   # a real buildGolferComment copies golfer names: random records give unterminated text (U-0945)
        img[goff:goff + gsz] = bytes(gsz)
    else:
        rand_obj(lib, img, "g_golfers", rng)
    for g in range(152):   # some golfers on this hole with a positive next byte (each takes one round off)
        if rng.random() < 0.05:
            img[goff + 0x100 * g + 0x21] = hole & 0xFF
            img[goff + 0x100 * g + 0x22] = rng.choice([0, 1, 5, 0x80])
    hoff, _ = objs(lib)["g_holes"]
    r = hoff + 0x208 * hole
    rec = bytearray(rng.randbytes(0x208))
    rec[0] = rng.choice([0, 1, 3, 4, 5, 9, 0xFE, rng.randrange(256)])
    struct.pack_into("<i", rec, 0x20, rng.choice([0, 1, 2, 3, 50, -1, -2, rng.randrange(-2 ** 31, 2 ** 31)]))
    struct.pack_into("<i", rec, 0x24, rng.choice([0, 0, 7, -9, rng.randrange(-200, 200)]))
    empty = rng.random() < 0.15   # an empty histogram (no stroke average line)
    for k in range(88):    # the histogram: small counts, some negative
        struct.pack_into("<h", rec, 0x28 + 2 * k, 0 if empty else rng.choice([0, 0, 1, 2, 5, -1, rng.randrange(-30, 60)]))
    if rng.random() < 0.15:   # a column whose count (8 + its words) is 0: its comparison line is skipped
        c = rng.choice([3, 5, 6, 7])
        for s in range(11):
            struct.pack_into("<h", rec, 0x28 + 2 * (c * 11 + s), -8 if s == 9 else 0)
    few = rng.random() < 0.3   # fewer than five positive counters: the empty rounds
    for k in range(0x40):  # the comment counters
        v = rng.choice([0, 0, 0, 1, 3, 9, -2, rng.randrange(-100, 400)])
        struct.pack_into("<h", rec, 0xD8 + 2 * k, (v if rng.random() < 0.05 else 0) if few else v)
    for k in range(0x40):  # the comment words; in the golfer-index range for a real buildGolferComment (U-0945)
        w = rng.randrange(0x98) if real_peer else rng.getrandbits(16)
        struct.pack_into("<H", rec, 0x16C + 2 * k, w)
    if rng.random() < 0.3:  # small denominators for the rating line
        struct.pack_into("<i", rec, 0x20, rng.choice([-4, -3, -5]))
        struct.pack_into("<i", rec, 0x24, rng.choice([0, 1, -1, 2]))
    elif rng.random() < 0.3:  # rating values in each word's band (v / 20 = 0, 1, 2, more; negative)
        struct.pack_into("<i", rec, 0x20, 6)
        struct.pack_into("<i", rec, 0x24, 4)    # denominator 2 + 6 + 4 = 12
        struct.pack_into("<h", rec, 0x158, rng.choice([1, 2, 3, -1, 5, 8]))
    img[r:r + 0x208] = rec
    return bytes(img)


def _inputs(rng):
    from port.test import cmdemu as CE
    out = []
    for _ in range(rng.choice([0, 1, 1, 2, 3])):
        k = rng.random()
        if k < 0.4:
            out += CE.click(rng.randrange(800), rng.randrange(600), rng.getrandbits(16), rng.getrandbits(16))
        elif k < 0.8:
            out += CE.key(rng.choice([0x1B, 0x41, 0x0D]), rng.choice([0, 0x61, 0x1B]))
        else:
            out.append(CE.Raw(rng.choice([2, 3, 9]), rng.randrange(800), rng.randrange(600)))
    return out


def cases(lib, n, seed, real_peer=False):
    rng = random.Random(f"holestats/{seed}/{real_peer}")
    out = []
    for _ in range(n):
        hole = rng.choice([1, 2, 5, 9, 17, 18, rng.randrange(0, 20)])
        img = _image(lib, rng, hole, real_peer)
        inputs = _inputs(rng)
        t0 = rng.getrandbits(32)
        script = _clock_script(rng, t0)
        arrive = [(t0 + rng.choice([0, 50, 1500, 5000, 40000, 2 ** 31])) & 0xFFFFFFFF for _ in _batches(inputs)]
        # the close button's box (0x23e..0x269, y + 8 .. + 0x2b; the page's y comes from sprite heights, 0 in the file's
        # image): its hover image is drawing only, the hover arms are still the original's branches
        cur = rng.choice([(rng.randrange(-10, 810), rng.randrange(-10, 610)), (0x250, 0x14), (0x23e, 8), (0x269, 0x33), (0x250, -5)])
        out.append((img, hole, inputs, script, arrive, cur))
    return out


# ------------------------------------------------------------------------------------------------ the two sides
def emu_run(case, real_peer=False, cover=None):
    from port.test import cmdemu as CE
    img, hole, inputs, script, arrive, cur = case
    emu = CE.Emu()
    for a in DROPPED:
        if a in NO_RET:
            emu.stub(a, 0, 0, False)
        else:
            emu.drop(a)
    for a, n in EH_VEC.items():
        emu.stub(a, n, 0, True)
    emu.stub(VIEW_TO_LOCAL, 2, ("cursor", cur), True)
    if not real_peer:
        emu.stub(BUILD_COMMENT, 4, 0, False, "buildGolferComment")
    by_ra = {SITES[k]: list(v) for k, v in script.items()}
    st = {"t": None, "k": 0}
    nb = len(_batches(inputs))

    def clock(ra):
        if ra not in by_ra:
            return None
        if not by_ra[ra]:
            raise CE.NeedInput(f"clock script of {ra:#x} ran out")
        st["t"] = by_ra[ra].pop(0)
        return st["t"]

    def gate():
        if st["k"] >= nb or st["t"] is None or st["t"] < arrive[st["k"]]:
            return False
        st["k"] += 1
        return True
    emu.stub(FAKE_DISPLAY + 0x800, 0, 0, False, "display slot")
    vt = struct.pack("<I", FAKE_DISPLAY + 0x100) + bytes(0xFC) + struct.pack("<I", FAKE_DISPLAY + 0x800) * 0x100
    block = (FAKE_DISPLAY, vt.ljust(0x900, b"\xC3"))
    return emu.run(ENTRY, [hole], img, inputs=inputs, blocks=[block], clock=clock, gate=gate, cover=cover)


def port_run(lib, case, real_peer=False):
    img, hole, inputs, script, arrive, cur = case
    with lib.state() as st:
        st.set_data_base(DATA_VA)
        st.load_image(img)
        words = [w for c in inputs for w in c.args()] or [0]
        cbuf = (C.c_uint32 * len(words))(*words)
        st.map(CMD_VA, cbuf, readonly=True)
        abuf = (C.c_uint32 * max(1, len(arrive)))(*(arrive or [0]))
        st.map(ARRIVE_VA, abuf, readonly=True)
        pairs = [w for name, vals in script.items() for v in vals for w in (SITE_OFF[name] & 0xFFFFFFFF, v)]
        kbuf = (C.c_uint32 * len(pairs))(*pairs)
        st.map(CLOCK_VA, kbuf, readonly=True)
        st.call("holestats.setScript", [CMD_VA, len(inputs), ARRIVE_VA, CLOCK_VA, len(pairs) // 2,
                                        cur[0] & 0xFFFFFFFF, cur[1] & 0xFFFFFFFF])
        logbuf = (C.c_uint32 * (1 + 8 * PEER_LOG_CAP))()
        st.map(PEER_LOG_VA, logbuf)
        if not real_peer:
            st.call("commands.recordPeer", [fnv1a("buildGolferComment"), PEER_LOG_VA, PEER_LOG_CAP])
        try:
            st.call("showHoleStatsScreen", [hole])
            code = 0
        except L.SgError as e:
            code = e.code
        used = st.call("holestats.consumed", []).eax
        st.call("holestats.clearScript", [])
        st.call("commands.recordClear", [])
        n = min(logbuf[0], PEER_LOG_CAP)
        calls = [tuple(logbuf[1 + 8 * k: 1 + 8 * k + 8]) for k in range(n)]
        return st.image(), code, used, calls


def compare(lib, case, real_peer=False) -> str:
    r = emu_run(case, real_peer)
    pimg, code, used, calls = port_run(lib, case, real_peer)
    if r.err.startswith("NeedInput"):
        if code != L.SG_E_NO_RESOURCE:
            return f"original waits for the clock, port code {code}"
    elif r.err:
        return "" if code in (L.SG_E_FAULT, L.SG_E_DIVIDE) else f"original error {r.err}, port code {code}"
    elif code:
        return f"port error {code}, original returned"
    if used != r.consumed:
        return f"input consumed port {used} original {r.consumed}"
    if not real_peer:
        orig = [(fnv1a(n), *[v & 0xFFFFFFFF for v in a]) for n, a in r.log if n == "buildGolferComment"]
        port = [(c[0], *c[1:1 + c[7]]) for c in calls]
        if orig != port:
            return f"buildGolferComment calls differ: port {len(port)} {port[:2]} original {len(orig)} {orig[:2]}"
    return first_diff(pimg, r.image, lib)


@need_exe
@emu_oracle
@pytest.mark.parametrize("real_peer", [False, True], ids=["recorded_peer", "real_peer"])
def test_show_hole_stats_screen_equals_emulated_original(portlib, real_peer):
    bad = []
    n = 300 if not real_peer else 120
    for k, case in enumerate(cases(portlib, n, 1, real_peer)):
        d = compare(portlib, case, real_peer)
        if d:
            bad.append((k, case[1], len(case[2]), d))
    assert not bad, f"{len(bad)} of {n} differ: {bad[:4]}"


@need_exe
@emu_oracle
def test_branch_coverage(portlib):
    """both sides of the jcc of showHoleStatsScreen's own body over the oracle cases (reported; the unreachable ones are
    listed in the notes)"""
    from port.test import cmdemu as CE
    cover = {"__functions__": [ENTRY]}
    for case in cases(portlib, 300, 1) + cases(portlib, 120, 1, True):
        emu_run(case, False, cover)
    n, both, missing = CE.coverage_summary(cover)
    print(f"\nshowHoleStatsScreen jcc: {both}/{n} both sides; missing {missing}")
    assert {a for a, _ in missing} <= UNREACHABLE, sorted({a for a, _ in missing} - UNREACHABLE)


# the jcc sides no case can reach: 31 `test eax, eax` on a register loaded with a fixed data address (the
# `p ? p : 0` idiom VC6 emits for the text buffer and the screen surface), `page > 4` after an unsigned `% 5` / `% 3`,
# `lo > lo + 5` (lo is par - 2 of a byte), and the hover image's `left the box` arm (the cursor of a case is fixed)
UNREACHABLE = {"0x4534c5", "0x45351b", "0x453594", "0x453606", "0x45363f", "0x4536b4", "0x453758", "0x45378f", "0x453843",
               "0x45387a", "0x45392c", "0x453963", "0x4539b9", "0x453a12", "0x453a8b", "0x453abc", "0x453b33", "0x453bcb",
               "0x453ccb", "0x453d5c", "0x453db0", "0x453de4", "0x453f1e", "0x453f4f", "0x453f80", "0x453fad", "0x454062",
               "0x454101", "0x45419d", "0x4541d1", "0x45425a", "0x454462", "0x4545e0"}


# ------------------------------------------------------------------------------------------------ golden digests
GOLDEN_N = 80


def _golden_cases(lib):
    return cases(lib, GOLDEN_N, "golden")


def _golden_port(lib):
    out = []
    for case in _golden_cases(lib):
        pimg, code, used, calls = port_run(lib, case)
        out.append(digest(None, pimg) + f":{code}:{used}:{len(calls)}:" + digest(None, json.dumps(calls).encode())[:8])
    return out


@need_exe
def test_golden_digests(portlib):
    """the port reproduces the emulated original's pinned results (image, error, consumed input, peer calls)"""
    if os.environ.get("SG_EMU_ORACLE") == "update":
        bad = [k for k, c in enumerate(_golden_cases(portlib)) if compare(portlib, c)]
        assert not bad, f"oracle RED on golden cases {bad[:5]}: not pinned"
        GOLDEN.write_text(json.dumps({"showHoleStatsScreen": _golden_port(portlib)}, indent=0, sort_keys=True))
    if not GOLDEN.exists():
        pytest.skip("no pinned digests (SG_EMU_ORACLE=update)")
    g = json.loads(GOLDEN.read_text())["showHoleStatsScreen"]
    got = _golden_port(portlib)
    bad = [k for k, (a, b) in enumerate(zip(got, g)) if a != b]
    assert not bad and len(got) == len(g), f"{len(bad)} golden cases differ: {bad[:6]}"


NEGCTL = [(1, "the page index from clock bit 9"), (2, "g_msgState53df54 left on the way out"),
          (3, "the histogram's last row stops at score 8")]


@need_exe
@pytest.mark.parametrize("mode,what", NEGCTL)
def test_negctl_reads_red(portlib, mode, what):
    """every wrong body changes a pinned digest (and, with the oracle, reads RED against the original)"""
    if not GOLDEN.exists():
        pytest.skip("no pinned digests")
    g = json.loads(GOLDEN.read_text())["showHoleStatsScreen"]
    portlib.negctl("showHoleStatsScreen", mode)
    try:
        got = _golden_port(portlib)
        if os.environ.get("SG_EMU_ORACLE") == "1":
            red = sum(bool(compare(portlib, c)) for c in _golden_cases(portlib))
            assert red > 0, f"mode {mode} ({what}) GREEN against the original"
    finally:
        portlib.negctl_reset()
    assert got != g, f"mode {mode} ({what}) is blind on the golden cases"


# ------------------------------------------------------------------------------------------------ the clock query
@need_exe
def test_without_a_clock_the_screen_waits_before_any_write(portlib):
    """no InputSource (or one without a clock): NeedInput at the entry read, before the screen writes anything"""
    img = _image(portlib, random.Random(3), 4, False)
    with portlib.state() as st:
        st.set_data_base(DATA_VA)
        st.load_image(img)
        with pytest.raises(L.SgError) as e:
            st.call("showHoleStatsScreen", [4])
        assert e.value.code == L.SG_E_NO_RESOURCE
        assert st.image() == img


@need_exe
def test_clock_reads_follow_the_script_site_by_site(portlib):
    """one build per page change: with the pass clock crossing a 1024 boundary every k reads, the port asks the entry
    site once, the after-draw site once per build, and the new-page site once per page change"""
    from port.test import cmdemu as CE
    img = bytearray(base_image(portlib))
    for n in ("g_rootInput", "g_redrawPending", "gDirtyFlagA", "gDirtyFlagB"):
        off, size = objs(portlib)[n]
        img[off:off + size] = bytes(size)
    hoff, _ = objs(portlib)["g_holes"]
    rec = hoff + 0x208 * 3
    img[rec:rec + 0x208] = bytes(0x208)
    img[rec] = 4                                          # par 4: no banner read
    struct.pack_into("<i", img, rec + 0x20, 7)            # rounds != 0: five pages
    for k, v in enumerate((10, 9, 8, 7, 6)):              # five comments per build
        struct.pack_into("<h", img, rec + 0xD8 + 2 * k, v)
    t0 = 0x10000                                          # page (t0 >> 10) % 5 = 4
    # pass read k (from 1) is t0 + 100 k: the page moves at reads 11, 21, 31; each move reads the new page
    # (t0 + 1024 m) and, after the build, the same value at the after-draw site
    script = {"entry": [t0], "after_draw": [t0 + 1024 * m for m in range(4)],
              "new_page": [t0 + 1024 * (m + 1) for m in range(3)], "pass": [t0 + 100 * (k + 1) for k in range(40)]}
    inputs = CE.key(0x1B, 0x1B)                           # onChar, onKey, onChar0: three batches
    arrive = [t0 + 3100] * len(_batches(inputs))
    _, code, used, calls = port_run(portlib, (bytes(img), 3, inputs, script, arrive, (0, 0)))
    # the fourth build's after-draw read is t0 + 3072 (not yet arrived); pass read 32 (t0 + 3200) lets the next pump
    # take onChar, whose code word ends the screen through flushRedraw: one command consumed, four builds of five
    assert (code, used) == (0, 1)
    assert len(calls) == 20 and calls[:5] * 4 == calls
    case = (bytes(img), 3, inputs, script, arrive, (0, 0))
    if os.environ.get("SG_EMU_ORACLE") in ("1", "update"):
        assert compare(portlib, case) == ""


# ------------------------------------------------------------------------------------------------ segments
need_segments = pytest.mark.skipif(not (ROOT / "port" / "corpus" / "segments").exists(), reason="no segment corpus")
SEGMENTS = ["championship/02", "cov_a_keys2/02"]


def _trace_reads(seg):
    sys.path.insert(0, str(ROOT / "re" / "tools"))
    import trace_format as TF
    tr = TF.read(ROOT / "port" / "corpus" / "segments" / seg / "trace.trace")
    want = set(SITES.values())
    return sum(r.f["count"] for r in tr.records if r.type == TF.CLOCKRUN and r.sub == 1 and r.f["ra"] in want)


@need_exe
@need_segments
@pytest.mark.parametrize("seg", SEGMENTS)
def test_segment_passes_with_every_clock_record_served(portlib, seg):
    """the segment PASSes, and the port took exactly the trace's clock records at the screen's five sites, in order,
    none missing and no dispatch delivered after a later clock read"""
    from port.test import segments as S
    r = S.SegmentRunner(portlib).run(S.load_segment(S.SEG_DIR / seg))
    assert r.status == "PASS", f"{seg}: {r.status} {r.frames_run}/{r.frames_expected} {r.detail} | {r.run_detail}"
    n = _trace_reads(seg)
    assert f"clock queries {n} served, 0 none left, 0 dispatches late" in r.run_detail, (n, r.run_detail)


@need_exe
@need_segments
@pytest.mark.parametrize("mode", [1, 2])
def test_segment_negctl_reads_red(portlib, mode):
    """on the segments: mode 1 (the page from clock bit 9) asks for clock reads the original never made, so the trace's
    records at those sites run out and the run stops in the screen; mode 2 (g_msgState53df54 not cleared) reopens the
    screen in the next frame, which has no clock records for it. Mode 3 changes only g_textBuffer (hidden class, not
    hashed): blind here, RED on the oracle and the golden digests."""
    from port.test import segments as S
    portlib.negctl("showHoleStatsScreen", mode)
    try:
        r = S.SegmentRunner(portlib).run(S.load_segment(S.SEG_DIR / "championship/02"))
    finally:
        portlib.negctl_reset()
    assert r.status != "PASS", r.run_detail
