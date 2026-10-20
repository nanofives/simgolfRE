"""Phase 3.8 stream 38c: module `reports` (port/src/sim/reports.cpp): the command effects of showCourseReport and
showEndOfYearScreen, split from their drawing as 3.7b did (log/p3/p37b_notes.txt; notes log/p38/38c_notes.md).

Evidence as in test_commands.py: the EMULATOR ORACLE (opt-in SG_EMU_ORACLE=1, port/test/cmdemu.py) runs the ORIGINAL function
under Unicorn with its drawing callees stubbed and the modal pumps fed by the scripted raw commands; eax, the consumed
input, the whole data image (minus the drawing scratch) and the peers recorded on both sides must equal the port's. The default
suite checks GOLDEN digests (port/test/golden_reports.json) pinned from the port after the oracle was GREEN on the same cases
(SG_EMU_ORACLE=update), and every negative control against them.
"""
from __future__ import annotations

import gc
import json
import os
import pathlib
import random
import struct

import pytest

from port.test import lib as L
from port.test import test_commands as TC

ROOT = pathlib.Path(__file__).resolve().parents[2]
GOLDEN = ROOT / "port" / "test" / "golden_reports.json"
pytestmark = pytest.mark.port
need_exe = pytest.mark.skipif(not L.exe_path().exists(), reason="the user's golf_clean.exe is not present")
emu_oracle = pytest.mark.skipif(os.environ.get("SG_EMU_ORACLE") not in ("1", "update"),
                                reason="opt-in: SG_EMU_ORACLE=1 runs the original under Unicorn (development oracle)")

ENTRY = {"showCourseReport": 0x0044FB30, "showEndOfYearScreen": 0x0044CFF0}
FUNCS = list(ENTRY)
# the callees the port drops, on top of the 3.7b screens' list: the audio requests (playSound, startSoundSlot)
DROPPED = TC.SCREEN_DROPPED + [0x004481B0, 0x00448200]
# peers recorded on both sides instead of run: the story file reader (file I/O) of the highlight list
RECORD = {"showEndOfYearScreen": {"loadStoryStrings": (0x00466B70, 4)}}
MASK = TC.SCREEN_MASK
HOLES = 0x00575AB0


# ------------------------------------------------------------------------------------------------ state generation
def _holes(lib, img, rng, q):
    """hole records 1..18 (0x208 bytes): a coherent course whose quality `q` (0..1) decides how often each field is good.
    Field meaning: +0 signed par, +4 length, +0x20 / +0x24 / +0x158 / +0x1ec pace and time inputs, +0x28 an 8 x 11 table of
    words (row r, column j at +0x28 + 2 (11 r + j)), +0xee / +0x104 / +0x110 words, +0x1f0 / +0x1f4 / +0x1f8 / +0x1fc
    dwords, +0x200 flag byte (showCourseReport's own header comment in shim/src/re/c3fr.cpp)."""
    off, _ = TC.objs(lib)["g_holes"]
    n = rng.choice([18, 18, 18, 9, 12, 0, 5])
    for h in range(1, 19):
        r = off + 0x208 * h
        img[r:r + 0x208] = bytes(0x208)
        if h > n and rng.random() < 0.9:
            continue
        good = lambda p=q: rng.random() < p   # noqa: E731
        par = rng.choice([3, 4, 5, 4]) if good(0.95) else rng.choice([0, 0, -3, 7, 100, 127, -128])
        img[r] = par & 0xFF
        struct.pack_into("<h", img, r + 4, 400 if good() else rng.randrange(-50, 700))
        r20 = 10 if good() else rng.choice([0, 1, 3, 40, -5])
        struct.pack_into("<i", img, r + 0x20, r20)
        struct.pack_into("<i", img, r + 0x24, 0 if good() else rng.randrange(-30, 200))
        struct.pack_into("<h", img, r + 0x158, rng.choice([16, 18, 20]) if good() else rng.randrange(-5, 40))
        struct.pack_into("<i", img, r + 0x1EC, 0 if good() else rng.randrange(0, 12000))
        for row in range(8):
            for col in range(11):
                v = 0
                if good(0.5) and col in (7, 8, 9) and row in (0, 1, 2, 3, 4, 5, 6):
                    v = rng.choice([10, 12, 30]) if good() else rng.randrange(-5, 40)
                elif rng.random() < (0.08 if q > 0.5 else 0.3):
                    v = rng.randrange(-3, 9)
                struct.pack_into("<h", img, r + 0x28 + 2 * (11 * row + col), v)
        struct.pack_into("<h", img, r + 0xEE, rng.choice([2, 4]) if good() else rng.randrange(-4, 12))
        struct.pack_into("<h", img, r + 0x104, rng.choice([4, 8]) if good() else rng.randrange(-4, 12))
        struct.pack_into("<h", img, r + 0x110, 10 if good() else rng.randrange(-4, 12))
        b, a, c = rng.choice([(100, 300, 50), (80, 500, 10), (40, -20, 5)])   # +0x1f0, +0x1f4, +0x1f8
        struct.pack_into("<iii", img, r + 0x1F0, b, a, c)
        struct.pack_into("<i", img, r + 0x1FC, 0 if good() else rng.choice([2, 3, 5, 1]))
        img[r + 0x200] = rng.getrandbits(8)
        for p in (0x8, 0xC):   # tee position words the tour events read (holes 9 and 18)
            struct.pack_into("<i", img, r + p, rng.randrange(-5, 60))


def _perfect_holes(lib, img, rng):
    """a course that earns the full 100 on the grading page: 18 holes of par 4 and length 400, no time or pace loss, the
    three difference tables far above the threshold; a few single-field perturbations then take marks away"""
    off, _ = TC.objs(lib)["g_holes"]
    for h in range(1, 19):
        r = off + 0x208 * h
        img[r:r + 0x208] = bytes(0x208)
        img[r] = 4
        struct.pack_into("<h", img, r + 4, 400)
        struct.pack_into("<i", img, r + 0x20, 10)
        struct.pack_into("<h", img, r + 0x158, 16)
        for row in (3, 5, 6):
            struct.pack_into("<h", img, r + 0x28 + 2 * (11 * row + 9), 10)
        struct.pack_into("<h", img, r + 0xEE, 4)
        struct.pack_into("<h", img, r + 0x104, 8)
        struct.pack_into("<h", img, r + 0x110, 10)
        struct.pack_into("<iii", img, r + 0x1F0, 100, 300, 50)
        img[r + 0x200] = rng.getrandbits(8)
        for p in (0x8, 0xC):
            struct.pack_into("<i", img, r + p, rng.randrange(-5, 60))
    for _ in range(rng.choice([0, 0, 1, 2, 3, 6])):
        r = off + 0x208 * rng.randrange(1, 19)
        which = rng.randrange(8)
        if which == 7:   # one slow hole: the summed time 297 makes the time criterion 0 stars, raised to 1
            struct.pack_into("<i", img, r + 0x1EC, 10 * 40 * 297)
        elif which == 0:
            struct.pack_into("<h", img, r + 4, rng.randrange(300, 500))
        elif which == 1:
            struct.pack_into("<i", img, r + 0x1EC, rng.randrange(0, 20000))
        elif which == 2:
            struct.pack_into("<h", img, r + 0x158, rng.randrange(0, 20))
        elif which == 3:
            struct.pack_into("<h", img, r + 0x28 + 2 * (11 * rng.choice([3, 5, 6]) + 9), rng.randrange(-8, 6))
        elif which == 4:
            struct.pack_into("<i", img, r + 0x1FC, rng.choice([2, 3]))
        elif which == 5:
            struct.pack_into("<h", img, r + 0x104, rng.randrange(-4, 4))
        else:
            img[r] = rng.choice([3, 5])


def _low_holes(lib, img, rng):
    """a course that scores low but above zero on the grading page (about 39 of 100): shorter holes (the length criterion keeps 7 stars), one slow hole, slow
    pace, and the last eight holes miss the table cells, the size sum and the pace bonus, so every count criterion keeps
    one or two stars"""
    _perfect_holes(lib, img, random.Random(0))
    off, _ = TC.objs(lib)["g_holes"]
    for h in range(1, 19):
        r = off + 0x208 * h
        struct.pack_into("<h", img, r + 4, rng.randrange(360, 401))
        struct.pack_into("<h", img, r + 0x158, rng.choice([8, 10, 12]))
        if h > 10:
            for row in (3, 5, 6):
                struct.pack_into("<h", img, r + 0x28 + 2 * (11 * row + 9), 0)
            for p in (0xEE, 0x104, 0x110):
                struct.pack_into("<h", img, r + p, 0)
            struct.pack_into("<i", img, r + 0x1FC, 5)
    struct.pack_into("<i", img, off + 0x208 + 0x1EC, 10 * 40 * rng.choice([290, 297, 280]))


def _special_tables(lib, img, rng):
    """tables the general generator rarely makes: every count zero (the cnt == 0 arms), a row whose weight sum cancels
    its start value 8 (the R94 == 0 arms)"""
    off, _ = TC.objs(lib)["g_holes"]
    mode = rng.choice(["zero", "cancel"])
    for h in range(1, 19):
        r = off + 0x208 * h
        if mode == "zero":
            img[r + 0x28:r + 0x28 + 2 * 88] = bytes(2 * 88)
        elif rng.random() < 0.5:
            row = rng.choice([3, 5, 6, 7, 0, 1])
            img[r + 0x28 + 2 * 11 * row:r + 0x28 + 2 * 11 * (row + 1)] = bytes(22)
            struct.pack_into("<h", img, r + 0x28 + 2 * (11 * row + rng.randrange(1, 10)), -8)


def _report_image(lib, rng, name):
    img = bytearray(TC._panel_image(lib, rng))

    def p32(nm, v, at=0):
        TC.put32(lib, img, nm, v, at)
    variant = rng.choice(["normal", "normal", "normal", "perfect", "perfect", "special", "low", "low"])
    variant = os.environ.get("SG_REPORT_VARIANT") or variant   # debugging: force one variant
    _holes(lib, img, rng, rng.choice([1.0, 0.97, 0.9, 0.7, 0.4, 0.1]))
    if variant == "perfect":
        _perfect_holes(lib, img, rng)
    elif variant == "special":
        _special_tables(lib, img, rng)
    elif variant == "low":
        _low_holes(lib, img, rng)
    p32("g_difficulty", rng.choice([0, 1, 2]))
    hp1 = {"perfect": lambda: rng.choice([19, 19, 19, 19, 10]), "low": lambda: 19}.get(
        variant, lambda: rng.choice([19, 19, 19, 12, 10, 6, 5, 1, 0, 20, 25]))()
    p32("g_holesPlusOne", hp1)
    mask = 0
    p32("g_flags", rng.getrandbits(32) & rng.choice([0xFFFFFFFF, 0x0041A040, 0x00402040, 0x01000000, 0x00400000]))
    if variant == "low":
        mask = 0x7F << 6
    p32("g_buildingKindMask", mask if variant == "low" else rng.getrandbits(32) if rng.random() < 0.4 else rng.choice([0xFFFFFFFF, 0x000FFFC0, 0x3F00, 0, 0x000FFFC0 | 1]))
    p32("g_courseReportScore", rng.randrange(0, 200))
    p32("g_tournamentPurse", rng.randrange(0, 5000))
    TC.put(lib, img, "g_ui_569498", bytes([rng.choice([0, 1, 2, 9])]))
    if rng.random() < 0.75:   # a quiet input state: the loop must wait for what the script delivers
        for nm in ("g_rootInput", "gDirtyFlagA", "gDirtyFlagB", "g_redrawPending", "g_flag_83afcc"):
            TC.put(lib, img, nm, bytes(TC.objs(lib)[nm][1]))
    # the display slot call and the surface slot of the drawing: a fake display, no surface
    struct.pack_into("<I", img, 0x00519CD8 - 0x004C1000, 0)
    struct.pack_into("<I", img, 0x0083AD50 - 0x004C1000, TC.FAKE_DISPLAY)
    if name == "showEndOfYearScreen":
        p32("g_date", rng.choice([rng.randrange(0, 60000), rng.randrange(8192 * 3, 8192 * 60), 1024 * 7 + 5, 1024 * 499 + 3,
                                  1024 * 500, rng.randrange(-3000, 0)]))
        TC.put(lib, img, "g_year", struct.pack("<h", rng.randrange(-5, 90)))
        p32("g_cash_div100", rng.choice([-1000, -5, 0, 7, 250000]))
        p32("g_debtYears", rng.choice([0, 1, 2, 3, 7, -1]))
        p32("g_eoy58b388", rng.getrandbits(32) if rng.random() < 0.3 else rng.randrange(0, 2000))
        for nm in ("g_cashHistory", "g_funHistory", "g_skillHistory", "g_memberTypeHistory"):
            TC.rand_obj(lib, img, nm, rng)
        eoff, _ = TC.objs(lib)["g_eventHistory"]
        kinds = [0, 0x20, 0x40, 0x60, 0x80, 0xA0, 0xC0, 0xE0, 0x100, 0x120, 0x140, 0x160]
        for k in range(500):   # the highlight events: kind (bits 5.. of the word) and subject (low 5 bits)
            struct.pack_into("<H", img, eoff + 2 * k, (rng.choice(kinds) | rng.randrange(32)) & 0xFFFF if rng.random() < 0.5 else
                             (rng.getrandbits(16) if rng.random() < 0.1 else 0))
        toff, _ = TC.objs(lib)["g_topScores"]
        for i in range(10):   # the score summary the screen ends with: the ten g_topScores rows, some empty
            r = toff + 0x9C * i
            img[r:r + 0x9C] = bytes(0x9C)
            if rng.random() < 0.6:
                img[r:r + 8] = bytes(rng.choice(b"ABCdefg") for _ in range(7)) + b"\0"
                img[r + 0x40:r + 0x48] = b"textrow\0"
                struct.pack_into("<iii", img, r + 0x80, rng.randrange(-50, 500), rng.randrange(-900, 9000),
                                 rng.randrange(-1000, 10 ** 7))
                struct.pack_into("<h", img, r + 0x96, rng.randrange(-2, 6))
    return bytes(img)


def _inputs(rng):
    from port.test import cmdemu as CE
    if rng.random() < 0.1:   # input that neither presses nor types: the loop runs another pass and then waits for more
        return [CE.Raw(rng.choice([2, 3, 9]), rng.randrange(800), rng.randrange(600)) for _ in range(rng.choice([1, 2]))]
    out = []
    for _ in range(rng.choice([1, 1, 2, 3])):
        k = rng.random()
        if k < 0.55:
            out += CE.click(rng.randrange(800), rng.randrange(600), rng.getrandbits(16), rng.getrandbits(16))
        elif k < 0.7:   # a right click: the end-of-year screen waits for the left button only
            out += [CE.Raw(4, rng.randrange(800), rng.randrange(600), rng.getrandbits(16), rng.getrandbits(16)), CE.Raw(3, 0, 0)]
        elif k < 0.9:
            out += CE.key(rng.choice([0x1B, 0x41, 0x50, 0x0D]), rng.choice([0, 0x61, 0x1B]))
        else:
            out.append(CE.Raw(rng.choice([2, 3, 9]), rng.randrange(800), rng.randrange(600)))
    return out


def cases(lib, name, n, seed):
    return list(iter_cases(lib, name, n, seed))


def iter_cases(lib, name, n, seed):
    """the cases one at a time (an image is 3.6 MB: a long run must not hold them all)"""
    rng = random.Random(f"{name}/{seed}")
    for k in range(n):
        img = _report_image(lib, rng, name)
        mode = rng.choice([0, 0, 1, 1, 2, 2]) if name == "showCourseReport" else None
        args = [mode] if mode is not None else []
        inputs = [] if mode == 2 else _inputs(rng)
        cur = (rng.randrange(-10, 810), rng.randrange(-10, 610))
        yield (img, args, inputs, cur)


# ------------------------------------------------------------------------------------------------ both sides
def emu(name, img, args, inputs, cur, cover=None):
    from port.test import cmdemu as CE
    e = CE.Emu()
    for a in DROPPED:
        if a in (0x00483D30, 0x00483C30, 0x00483C70, 0x00483BD0):   # tail-jumping helpers without arguments
            e.stub(a, 0, 0, False)
        else:
            e.drop(a)
    for a, n in TC.EH_VEC.items():
        e.stub(a, n, 0, True)
    e.stub(TC.VIEW_TO_LOCAL, 2, ("cursor", cur), True)
    for nm, (a, n) in RECORD.get(name, {}).items():
        e.stub(a, n, 0, False, nm)
    e.stub(TC.FAKE_DISPLAY + 0x800, 0, 0, False, "display slot")
    e.stub(TC.FAKE_DISPLAY + 0x820, 3, 0, True, "display slot 0x9c")   # showCourseReport's call: three arguments, ret 0xc
    slots = [TC.FAKE_DISPLAY + 0x800] * 0x100
    slots[0x9C // 4] = TC.FAKE_DISPLAY + 0x820
    vt = struct.pack("<I", TC.FAKE_DISPLAY + 0x100) + bytes(0xFC) + struct.pack("<256I", *slots)
    block = (TC.FAKE_DISPLAY, vt.ljust(0x900, b"\xC3"))
    r = e.run(ENTRY[name], args, img, inputs=inputs, blocks=[block], cover=cover)
    gc.collect()   # a Unicorn instance and its hook closures form a cycle: free the 3.6 MB images now
    return r


def port(lib, name, img, args, inputs, cur):
    import ctypes as C
    with lib.state() as st:
        st.set_data_base(TC.DATA_VA)
        st.load_image(img)
        words = [w for c in inputs for w in c.args()]
        if inputs:
            buf = (C.c_uint32 * len(words))(*words)
            st.map(TC.INPUT_VA, buf, readonly=True)
        st.call("commands.setInput", [TC.INPUT_VA, len(inputs)])
        st.call("commands.setCursor", [cur[0] & 0xFFFFFFFF, cur[1] & 0xFFFFFFFF])
        logbuf = (C.c_uint32 * (1 + 8 * TC.PEER_LOG_CAP))()
        st.map(TC.PEER_LOG_VA, logbuf)
        for nm in RECORD.get(name, {}):
            st.call("commands.recordPeer", [TC.fnv1a(nm), TC.PEER_LOG_VA, TC.PEER_LOG_CAP])
        try:
            eax, code = st.call(name, list(args)).eax, 0
        except L.SgError as e:
            eax, code = None, e.code
        used = st.call("commands.inputConsumed", []).eax
        st.call("commands.clearInput", [])
        st.call("commands.recordClear", [])
        n = min(logbuf[0], TC.PEER_LOG_CAP)
        port.calls = [tuple(logbuf[1 + 8 * k: 1 + 8 * k + 8]) for k in range(n)]
        return eax, st.image(), code, used


def compare(lib, name, case) -> str:
    img, args, inputs, cur = case
    r = emu(name, img, args, inputs, cur)
    pe, pimg, code, used = port(lib, name, img, args, inputs, cur)
    if r.err.startswith("NeedInput"):
        return "" if code == L.SG_E_NO_RESOURCE else f"original waits for input, port {code} {pe}"
    if r.err:
        return "" if pe is None and code in (L.SG_E_FAULT, L.SG_E_DIVIDE) else f"original error {r.err}, port {pe} {code}"
    if pe is None:
        return f"port error {code}, original eax {r.eax:#x}"
    if used != r.consumed:
        return f"input consumed port {used} original {r.consumed}"
    rec = RECORD.get(name, {})
    if rec:
        orig = [(TC.fnv1a(n), *[v & 0xFFFFFFFF for v in a]) for n, a in r.log if n in rec]
        pc = [(c[0], *c[1:1 + c[7]]) for c in port.calls]
        if orig != pc:
            return f"peer calls differ: port {pc[:3]} original {orig[:3]}"
    b = bytearray(pimg), bytearray(r.image)
    for im in b:
        for nm, at, ln in MASK:
            if nm in TC.objs(lib):
                off, size = TC.objs(lib)[nm]
                ln2 = size - at if ln is None else ln
                im[off + at:off + at + ln2] = bytes(ln2)
    return TC.first_diff(bytes(b[0]), bytes(b[1]), lib)


@need_exe
@emu_oracle
@pytest.mark.parametrize("name", FUNCS)
def test_report_equals_emulated_original(portlib, name):
    bad = []
    for k, case in enumerate(iter_cases(portlib, name, int(os.environ.get("SG_REPORT_N", "150")), 1)):
        d = compare(portlib, name, case)
        if d:
            bad.append((k, case[1], len(case[2]), d))
    assert not bad, f"{len(bad)} differ: {bad[:4]}"


# ------------------------------------------------------------------------------------------------ golden digests
GOLDEN_N = 150
NEGCTL_RED = [
    ("showCourseReport", 1, "the purse uses (score - 50) / 5 + 4"),
    ("showCourseReport", 2, "a passing report sets only flag bit 22"),
    ("showCourseReport", 3, "the full-marks event is 0x14"),
    ("showCourseReport", 4, "the facility count starts at bit 7"),
    ("showCourseReport", 5, "a zero-star facility criterion does not zero the report"),
    ("showCourseReport", 6, "the leave stores 1 in the mode byte"),
    ("showEndOfYearScreen", 1, "the member history stores the live count + 1"),
    ("showEndOfYearScreen", 2, "the debt counter is not cleared outside debt"),
    ("showEndOfYearScreen", 3, "a right click ends the screen too"),
    ("showEndOfYearScreen", 4, "event 0x120 passes the subject + 1 to loadStoryStrings"),
    ("showEndOfYearScreen", 5, "addScore is not asked after the loop"),
]


def _golden_cases(lib, name):
    key = ("golden", name)
    if key not in TC._BASE:
        TC._BASE[key] = cases(lib, name, GOLDEN_N, "golden")
    return TC._BASE[key]


def _golden_port(lib, name):
    out = []
    for img, args, inputs, cur in _golden_cases(lib, name):
        pe, pimg, code, used = port(lib, name, img, args, inputs, cur)
        calls = repr(port.calls)
        out.append(TC.digest(pe, pimg) + f":{code}:{used}:" + TC.digest(0, calls.encode())[:8])
    return out


def _golden_load():
    return json.loads(GOLDEN.read_text()) if GOLDEN.exists() else {}


@need_exe
@pytest.mark.parametrize("name", FUNCS)
def test_golden_digests(portlib, name):
    if os.environ.get("SG_EMU_ORACLE") == "update":
        bad = [k for k, c in enumerate(_golden_cases(portlib, name)) if compare(portlib, name, c)]
        assert not bad, f"oracle RED on golden cases {bad[:5]}: not pinned"
        g = _golden_load()
        g[name] = _golden_port(portlib, name)
        GOLDEN.write_text(json.dumps(g, indent=0, sort_keys=True))
    g = _golden_load()
    if name not in g:
        pytest.skip(f"{name}: no pinned digests (SG_EMU_ORACLE=update)")
    got = _golden_port(portlib, name)
    bad = [k for k, (a, b) in enumerate(zip(got, g[name])) if a != b]
    assert not bad and len(got) == len(g[name]), f"{len(bad)} golden cases differ: {bad[:6]}"


@need_exe
@pytest.mark.parametrize("name,mode,what", NEGCTL_RED)
def test_negctl_reads_red(portlib, name, mode, what):
    g = _golden_load()
    if name not in g:
        pytest.skip(f"{name}: no pinned digests")
    portlib.negctl(name, mode)
    try:
        got = _golden_port(portlib, name)
        if os.environ.get("SG_EMU_ORACLE") == "1":
            red = sum(bool(compare(portlib, name, c)) for c in _golden_cases(portlib, name))
            assert red > 0, f"{name} mode {mode} ({what}) GREEN against the original"
    finally:
        portlib.negctl_reset()
    assert got != g[name], f"{name} mode {mode} ({what}) is blind on the golden cases"


def test_every_report_function_has_a_negctl(portlib):
    mine = {n for n, f in portlib.functions().items() if f.get("module") == "reports"}
    assert mine == set(FUNCS)
    assert {n for n, *_ in NEGCTL_RED} == mine
