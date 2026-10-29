"""Roadmap S2.10: module `buyland` (port/src/sim/buyland.cpp), showBuyLandScreen (TRACTS FOR SALE), its command effect split from its
drawing as 3.7b did (log/p3/p37b_notes.txt; notes log/roadmap/S2/10_notes.md).

Evidence as in test_routing.py: the EMULATOR ORACLE (opt-in SG_EMU_ORACLE=1, port/test/cmdemu.py) runs the ORIGINAL function under
Unicorn with its drawing callees stubbed and the modal pump fed by the scripted raw commands; eax, the consumed input and the whole
data image (minus the drawing scratch) must equal the port's. cellAt stays real on both sides (a pure lookup whose result the screen
stores), so does showRoutingMapScreen in its draw-only mode (the routing module's port, judged by test_routing.py), Random::range,
appendNumber, canAfford (its drawWindow stubbed, no golfer selected) and logTick. The default suite checks GOLDEN digests
(port/test/golden_buyland.json) pinned from the port after the oracle was GREEN on the same cases (SG_EMU_ORACLE=update), and every
negative control against them.
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
from port.test import test_routing as TR

ROOT = pathlib.Path(__file__).resolve().parents[2]
GOLDEN = ROOT / "port" / "test" / "golden_buyland.json"
pytestmark = pytest.mark.port
need_exe = pytest.mark.skipif(not L.exe_path().exists(), reason="the user's golf_clean.exe is not present")
emu_oracle = pytest.mark.skipif(os.environ.get("SG_EMU_ORACLE") not in ("1", "update"),
                                reason="opt-in: SG_EMU_ORACLE=1 runs the original under Unicorn (development oracle)")

NAME = "showBuyLandScreen"
ENTRY = 0x004587A0
CELL_AT = 0x0043D6F0
# the drawing callees the port drops: the 3.7b screens' list and the routing map's (cellAt kept real), plus the text-block and
# right-aligned text drawers, drawWindow (canAfford's message: its thought bubble is tested apart, no golfer selected here) and
# playSound (the audio backend's)
DROPPED = sorted({a for a in TR.DROPPED if a != CELL_AT} | {0x00478430, 0x00478080, 0x0040D320, 0x004481B0})
# drawing scratch the original writes and the port does not (the 3.7b screens' mask and the line point)
MASK = TC.SCREEN_MASK + [("g_linePoint5a412c", 0, None)]
# the lots' hit regions (x ranges of the three columns, the 68-row bands from 0x36) and the done button
COLS = [(0xF, 0x106), (0x110, 0x20C), (0x21A, 0x307)]


# ------------------------------------------------------------------------------------------------ state generation
def _buy_image(lib, rng, cur):
    img = bytearray(TR._routing_image(lib, rng, cur, bad_golfer=False))

    def p32(nm, v, at=0):
        TC.put32(lib, img, nm, v, at)
    o = TC.objs(lib)
    p32("f_w_4c2e00", -1, 8)   # no golfer selected: canAfford's drawWindow composes nobody
    # the root surface's font word (+0x5c of g_rootWindow + 0x274) as setDrawTarget(g_fontRef_821ee8) leaves it before the lots: the
    # original reads that FontRef's height fields for the price label's row (drawing only); setDrawTarget itself is stubbed
    p32("g_rootWindow", TC.DATA_VA + o["g_fontRef_821ee8"][0], 0x274 + 0x5C)
    if rng.random() < 0.5:   # that font's height words (+8 signed, +0xc, +0x10): both arms of the label row's choice
        p32("g_fontRef_821ee8", rng.choice([-1, -2, 0, 3, 12]), 8)
        p32("g_fontRef_821ee8", rng.randrange(0, 30), 0xC)
        p32("g_fontRef_821ee8", rng.randrange(0, 30), 0x10)
    p32("g_landPurchaseCount", rng.choice([0, 0, 1, 2, 3, 5, 9, 31, 32, rng.randrange(-5, 70)]))
    p32("g_cash_div100", rng.choice([0, 10, 400, 2000, 10 ** 6, rng.randrange(-100, 30000)]))
    p32("g_holesPlusOne", rng.choice([1, 3, 4, 5, 12, 19]))
    if rng.random() < 0.3:
        p32("g_flags", rng.getrandbits(32) | (1 << 24))
    elif rng.random() < 0.6:
        p32("g_flags", rng.getrandbits(32) & ~(1 << 24))
    TC.put(lib, img, "g_year", struct.pack("<h", rng.choice([0, 1, 5, 40, 99, 100, 101, 250, rng.randrange(0, 600)])))
    # the tile map by lot: whole lots for sale, none, or a mixture; the saved types mostly real types, a few out of range
    toff, _ = o["g_tileType"]
    soff, _ = o["g_tileTypeSaved"]
    out_of_range = rng.random() < 0.08
    for lot in range(9):
        q, r = lot // 3, lot % 3
        kind = rng.choice(["all", "none", "mix", "mix", "few"])
        for j in range(16):
            for i in range(16):
                idx = (r * 16 + 1 + i) * 50 + (q * 16 + 1 + j)
                p = {"all": 1.0, "none": 0.0, "mix": 0.5, "few": 0.03}[kind]
                img[toff + idx] = 0x14 if rng.random() < p else rng.choice([0, 1, 2, 3, 0xD, 0x11])
                t = rng.choice([1, 1, 2, 3, 4, 0xD, 0xE, 0x11, rng.randrange(23)])
                if out_of_range and rng.random() < 0.02:
                    t = rng.choice([-1, -3, -10, 23, 25, 31])
                img[soff + idx] = t & 0xFF
    # the type records' class byte that picks the record's second name (0xd)
    aoff, _ = o["g_tileTypeAttrs"]
    for t in range(23):
        img[aoff + 0x26 + 0x30 * t] = rng.choice([0, 0xD, 0xD, 1, 7])
        name = rng.choice([b"rough", b"trees", b"water", b"pine trees", b"deep rough", b"wetlands"])
        img[aoff + 0x30 * t:aoff + 0x30 * t + 0x10] = name.ljust(0x10, b"\0")
        img[aoff + 0x30 * t + 0x10:aoff + 0x30 * t + 0x20] = (name + b"2").ljust(0x10, b"\0")
    # the cell sheets cellAt reads (first cells and widths), including -1 (no sheet) and widths at or below the column
    foff, fsz = o["g_sheetFirstCell"]
    first = [rng.choice([-1, 0, 5, 100, rng.randrange(2000)]) for _ in range(fsz // 4)]
    struct.pack_into(f"<{len(first)}i", img, foff, *first)
    TC.put(lib, img, "g_courseType", struct.pack("<b", rng.choice([0, 1, 2, 3, -1])))
    if rng.random() < 0.8:   # a quiet input state: the loop must wait for what the script delivers
        for nm in ("g_rootInput", "gDirtyFlagA", "gDirtyFlagB", "g_redrawPending", "g_flag_83afcc"):
            TC.put(lib, img, nm, bytes(TC.objs(lib)[nm][1]))
    if rng.random() < 0.1:   # a button word other than 1 / 2 left set with the click flag (the pump keeps it): the region rule
        p32("g_rootInput", rng.choice([3, 4, -1, 0x10]), 4 * 4)
        p32("g_rootInput", 1, 4 * 7)
    return bytes(img)


def _cursor(rng):
    k = rng.random()
    if k < 0.75:   # on a lot
        lo, hi = rng.choice(COLS)
        return rng.randrange(lo, hi), rng.randrange(0x36, 0x102)
    if k < 0.85:   # the done button
        return rng.randrange(0x296, 0x2D6), rng.randrange(0x215, 0x255)
    return rng.randrange(-10, 810), rng.randrange(-10, 610)


def _inputs(rng):
    from port.test import cmdemu as CE
    out = []
    for _ in range(rng.choice([1, 1, 2, 3, 4])):
        k = rng.random()
        if k < 0.55:    # a left click (the screen acts on the region under the cursor, not on the click point)
            out += CE.click(rng.randrange(800), rng.randrange(600))
        elif k < 0.7:   # a key: nothing for this screen
            out += CE.key(rng.choice([0x1B, 0x41, 0x0D]), rng.choice([0, 0x61, 0x1B]))
        elif k < 0.85:  # a right click: leaves without a purchase
            out += [CE.Raw(4, rng.randrange(800), rng.randrange(600), 0, 0), CE.Raw(3, 0, 0)]
        else:
            out.append(CE.Raw(rng.choice([2, 3, 9]), rng.randrange(800), rng.randrange(600)))
    if rng.random() < 0.5:   # end with a right click when the script did not end the screen
        out += [CE.Raw(4, 10, 10, 0, 0), CE.Raw(3, 0, 0)]
    return out


def boundary_cases(lib):
    """the cursor on and around the edges of the lot columns, the band borders and the done button, then a right click"""
    from port.test import cmdemu as CE
    rng = random.Random(f"{NAME}/boundary")
    xs = [0xE, 0xF, 0x105, 0x106, 0x10F, 0x110, 0x20B, 0x20C, 0x219, 0x21A, 0x306, 0x307]
    ys = [0x35, 0x36, 0x36 + 67, 0x36 + 68, 0x36 + 135, 0x36 + 136, 0x101, 0x102]
    out = []
    for x in xs:
        for y in ys:
            out.append((_buy_image(lib, rng, (x, y)), [], CE.click(5, 5) + [CE.Raw(4, 10, 10, 0, 0), CE.Raw(3, 0, 0)], (x, y)))
    for x in (0x295, 0x296, 0x2D5, 0x2D6):
        for y in (0x214, 0x215, 0x254, 0x255):
            out.append((_buy_image(lib, rng, (x, y)), [], CE.click(5, 5) + [CE.Raw(4, 10, 10, 0, 0), CE.Raw(3, 0, 0)], (x, y)))
    return out


def cases(lib, n, seed):
    return list(iter_cases(lib, n, seed))


def iter_cases(lib, n, seed):
    rng = random.Random(f"{NAME}/{seed}")
    for _ in range(n):
        cur = _cursor(rng)
        yield (_buy_image(lib, rng, cur), [], _inputs(rng), cur)


# ------------------------------------------------------------------------------------------------ both sides
def emu(img, args, inputs, cur, cover=None):
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
    e.stub(TC.FAKE_DISPLAY + 0x800, 0, 0, False, "display slot")
    e.stub(TC.FAKE_DISPLAY + 0x820, 3, 0, True, "display slot 0x9c")
    slots = [TC.FAKE_DISPLAY + 0x800] * 0x100
    slots[0x9C // 4] = TC.FAKE_DISPLAY + 0x820
    vt = struct.pack("<I", TC.FAKE_DISPLAY + 0x100) + bytes(0xFC) + struct.pack("<256I", *slots)
    block = (TC.FAKE_DISPLAY, vt.ljust(0x900, b"\xC3"))
    r = e.run(ENTRY, args, img, inputs=inputs, blocks=[block], cover=cover)
    gc.collect()
    return r


def port(lib, img, args, inputs, cur):
    import ctypes as C
    with lib.state() as st:
        if st.lib.has("audio_exe.stubRequests"):  # S4: the audio requests stubbed, as this file's oracle stubs playSound
            st.call("audio_exe.stubRequests", [1])
        st.set_data_base(TC.DATA_VA)
        st.load_image(img)
        words = [w for c in inputs for w in c.args()]
        if inputs:
            buf = (C.c_uint32 * len(words))(*words)
            st.map(TC.INPUT_VA, buf, readonly=True)
        st.call("commands.setInput", [TC.INPUT_VA, len(inputs)])
        st.call("commands.setCursor", [cur[0] & 0xFFFFFFFF, cur[1] & 0xFFFFFFFF])
        try:
            eax, code = st.call(NAME, list(args)).eax, 0
        except L.SgError as e:
            eax, code = None, e.code
        used = st.call("commands.inputConsumed", []).eax
        st.call("commands.clearInput", [])
        return eax, st.image(), code, used


def compare(lib, case) -> str:
    img, args, inputs, cur = case
    r = emu(img, args, inputs, cur)
    pe, pimg, code, used = port(lib, img, args, inputs, cur)
    if r.err.startswith("NeedInput"):
        return "" if code == L.SG_E_NO_RESOURCE and pe is None else f"original waits for input, port {code} {pe}"
    if r.err:
        return "" if pe is None and code in (L.SG_E_FAULT, L.SG_E_DIVIDE) else f"original error {r.err}, port {pe} {code}"
    if pe is None:
        return f"port error {code}, original eax {r.eax:#x}"
    if used != r.consumed:
        return f"input consumed port {used} original {r.consumed}"
    if pe != r.eax:
        return f"eax port {pe:#x} original {r.eax:#x}"
    b = bytearray(pimg), bytearray(r.image)
    hoff = TC.objs(lib)["g_holes"][0]
    if not any(img[hoff + 0x208 * h] for h in range(1, 19)):
        # no hole drawn: the last write of g_map822b80[1] is the routing map's draw pass, whose cell pointer the routing port
        # does not write (test_routing.py masks it); with a hole the screen's own cellAt result is the last write and compared
        for im in b:
            off = TC.objs(lib)["g_map822b80"][0]
            im[off + 4:off + 8] = bytes(4)
    for im in b:
        for nm, at, ln in MASK:
            if nm in TC.objs(lib):
                off, size = TC.objs(lib)[nm]
                ln2 = size - at if ln is None else ln
                im[off + at:off + at + ln2] = bytes(ln2)
    return TC.first_diff(bytes(b[0]), bytes(b[1]), lib)


def _frame_type_fault(lib, case) -> bool:
    """a saved type outside -10 .. 31 counted for a tile for sale: the original counts into frame words the port does not model"""
    img = case[0]
    o = TC.objs(lib)
    toff, soff = o["g_tileType"][0], o["g_tileTypeSaved"][0]
    for lot in range(9):
        q, r = lot // 3, lot % 3
        for j in range(16):
            for i in range(16):
                idx = (r * 16 + 1 + i) * 50 + (q * 16 + 1 + j)
                t = struct.unpack("<b", img[soff + idx:soff + idx + 1])[0]
                if img[toff + idx] == 0x14 and not -10 <= t <= 31:
                    return True
    return False


@need_exe
@emu_oracle
def test_buy_land_equals_emulated_original(portlib):
    bad, cover = [], {}
    for k, case in enumerate(iter_cases(portlib, int(os.environ.get("SG_BUYLAND_N", "150")), 1)):
        d = compare(portlib, case)
        if d and not (d.startswith("port error") and _frame_type_fault(portlib, case)):
            bad.append((k, len(case[2]), case[3], d))
    assert not bad, f"{len(bad)} differ: {bad[:4]}"


@need_exe
@emu_oracle
def test_hit_edges_equal_emulated_original(portlib):
    bad = []
    for k, case in enumerate(boundary_cases(portlib)):
        d = compare(portlib, case)
        if d:
            bad.append((k, case[3], d))
    assert not bad, f"{len(bad)} differ: {bad[:4]}"


@need_exe
@emu_oracle
def test_branch_coverage(portlib):
    """every conditional jump of the original's body taken both ways over the random and boundary cases, except those listed"""
    from port.test import cmdemu as CE
    cover = {"__functions__": [ENTRY]}
    for case in list(iter_cases(portlib, int(os.environ.get("SG_BUYLAND_N", "150")), 1)) + boundary_cases(portlib):
        img, args, inputs, cur = case
        emu(img, args, inputs, cur, cover=cover)
    n, both, missing = CE.coverage_summary(cover)
    print(f"jcc sites {n}, both sides {both}, missing {missing}")
    assert both >= n - len(UNREACHED), f"{n - both} sites one-sided: {missing}"


# conditional jumps one side of which no case reaches (filled from test_branch_coverage's report, each with its reason)
UNREACHED = [
    (0x00458B02, "`test ecx, ecx` of the string constant drawText takes: never zero"),
    (0x00458D2E, "`test eax, eax` of the label's frame address: never zero"),
    (0x004590C2, "`test eax, eax` of the text buffer's address: never zero"),
    (0x004592B1, "the redraw flag after the input loop: every way into the loop passed the redraw block, which clears it"),
    (0x004592F6, "a cost of 0: the first pass stores worth * 10 with worth >= 1 (sum * 20 / 100 of a sum >= 5, whose wrapped "
                 "multiples of 2^s * 20 keep the product >= 100 or negative), the frame word below the costs holds the last lot's "
                 "row number 2 (only ever incremented by a negative type's counter)"),
    (0x00459398, "the purchase loop's `jle` on the lot size constant 16"),
]


# ------------------------------------------------------------------------------------------------ golden digests
GOLDEN_N = 120
NEGCTL_RED = [
    (1, "the first pass stores the worth, not worth * 10, as the cost"),
    (2, "the hit test's bands are 67 rows, not 68"),
    (3, "the purchase leaves the tiles for sale alone"),
    (4, "no Random::range draw per tile for sale"),
    (5, "logTick gets 0x141"),
    (6, "a lot is worth sum * 20 / 99"),
    (7, "a right click buys like a left click"),
    (8, "the finance record of the year + 1 loses the price"),
]


def _golden_cases(lib):
    key = ("golden", NAME)
    if key not in TC._BASE:
        TC._BASE[key] = cases(lib, GOLDEN_N, "golden") + boundary_cases(lib)
    return TC._BASE[key]


def _golden_port(lib):
    out = []
    for img, args, inputs, cur in _golden_cases(lib):
        pe, pimg, code, used = port(lib, img, args, inputs, cur)
        out.append(TC.digest(pe, pimg) + f":{code}:{used}")
    return out


def _golden_load():
    return json.loads(GOLDEN.read_text()) if GOLDEN.exists() else {}


@need_exe
def test_golden_digests(portlib):
    if os.environ.get("SG_EMU_ORACLE") == "update":
        bad = [k for k, c in enumerate(_golden_cases(portlib)) if compare(portlib, c)]
        assert not bad, f"oracle RED on golden cases {bad[:5]}: not pinned"
        g = _golden_load()
        g[NAME] = _golden_port(portlib)
        GOLDEN.write_text(json.dumps(g, indent=0, sort_keys=True))
    g = _golden_load()
    if NAME not in g:
        pytest.skip(f"{NAME}: no pinned digests (SG_EMU_ORACLE=update)")
    got = _golden_port(portlib)
    bad = [k for k, (a, b) in enumerate(zip(got, g[NAME])) if a != b]
    assert not bad and len(got) == len(g[NAME]), f"{len(bad)} golden cases differ: {bad[:6]}"


@need_exe
@pytest.mark.parametrize("mode,what", NEGCTL_RED)
def test_negctl_reads_red(portlib, mode, what):
    g = _golden_load()
    if NAME not in g:
        pytest.skip(f"{NAME}: no pinned digests")
    portlib.negctl(NAME, mode)
    try:
        got = _golden_port(portlib)
        if os.environ.get("SG_EMU_ORACLE") == "1":
            red = sum(bool(compare(portlib, c)) for c in _golden_cases(portlib))
            assert red > 0, f"{NAME} mode {mode} ({what}) GREEN against the original"
    finally:
        portlib.negctl_reset()
    assert got != g[NAME], f"{NAME} mode {mode} ({what}) is blind on the golden cases"


def test_buyland_module_registers_the_screen(portlib):
    mine = {n for n, f in portlib.functions().items() if f.get("module") == "buyland"}
    assert mine == {NAME, "syncTiles"}


# ------------------------------------------------------------------------------------------------ the pinned scenario
# cov_r_s2_screens (re/tools/scenario_s2_6.py) ends with the land popup and showBuyLandScreen, left with a right click (no
# purchase: the purchase is judged by the oracle cases above only). The screen runs in the frame of record 1155; record 1156 is the
# next frame head (the screen's RNG draws and the price texts' appendNumber, which writes g_textBuf); syncTiles runs after the
# screen whatever it returns, and its g_terrain820f2a byte shows at record 1157 (the next updateTerrainRender's path decides bit
# 1 of g_tileWallMask[0]).
SCEN = "cov_r_s2_screens"


def _boot_need():
    from port.test import boot as B
    if not (B.TRACES / f"{SCEN}.trace").exists() or B.expected_records(SCEN)[1] is None:
        pytest.skip(f"no trace or capture for {SCEN}")


@pytest.mark.parametrize("name,mode,record,objects", [
    ("showBuyLandScreen", 4, 1156, {"g_rng"}),        # no Random::range draw per tile for sale
    ("showBuyLandScreen", 1, 1156, {"g_textBuf"}),    # the cost is the worth, not worth * 10 (the price text's number)
    ("syncTiles", 1, 1157, {"g_tileWallMask"}),       # g_terrain820f2a[0] not set: the next render takes the incremental path
])
def test_screen_controls_read_red_in_the_boot(portlib, name, mode, record, objects):
    """the boot equals the capture through the land popup's frame; each control makes the named record differ"""
    from port.test import boot as B
    _boot_need()
    portlib.negctl(name, mode)
    try:
        r = B.run(SCEN, portlib, max_records=record + 1)
    finally:
        portlib.negctl(name, 0)
    d = r.first_divergence or {}
    assert d.get("record") == record and objects <= set(d.get("objects", [])), f"{name} {mode}: {r.status} {r.detail}"
