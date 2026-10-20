"""Phase 3.8 stream 38g: module `routing` (port/src/sim/routing.cpp): the command effect of showRoutingMapScreen (the F5 routing
map), split from its drawing as 3.7b did (log/p3/p37b_notes.txt; notes log/p38/38g_notes.md).

Evidence as in test_reports.py: the EMULATOR ORACLE (opt-in SG_EMU_ORACLE=1, port/test/cmdemu.py) runs the ORIGINAL function under
Unicorn with its drawing callees stubbed and the modal pump fed by the scripted raw commands; eax, the consumed input and the
whole data image (minus the drawing scratch) must equal the port's. The default suite checks GOLDEN digests
(port/test/golden_routing.json) pinned from the port after the oracle was GREEN on the same cases (SG_EMU_ORACLE=update), and
every negative control against them.
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
GOLDEN = ROOT / "port" / "test" / "golden_routing.json"
pytestmark = pytest.mark.port
need_exe = pytest.mark.skipif(not L.exe_path().exists(), reason="the user's golf_clean.exe is not present")
emu_oracle = pytest.mark.skipif(os.environ.get("SG_EMU_ORACLE") not in ("1", "update"),
                                reason="opt-in: SG_EMU_ORACLE=1 runs the original under Unicorn (development oracle)")

NAME = "showRoutingMapScreen"
ENTRY = 0x00456BE0
# the drawing callees the port drops, on top of the 3.7b screens' list: the line / polygon / text-block drawers and the
# per-tile value helpers whose results only choose a colour (mapToScreen, overlayCharAt, tileBlocked, clamp, foldRange and
# absDiff stay real: they are the pure helpers the redraw calls, so any write of theirs would show as a difference)
DROPPED = TC.SCREEN_DROPPED + [0x0040BF00, 0x0040BF20, 0x00475DF0, 0x00478530, 0x00404BC0, 0x0042EF40, 0x0040DB90, 0x0042DBA0]
# drawing scratch the original writes and the port does not: the line point, the last cell pointer, plus the 3.7b screens' mask
MASK = TC.SCREEN_MASK + [("g_linePoint5a412c", 0, None), ("g_map822b80", 4, 4)]
GOLFERS_HOLE_BYTE = 0x21
MODES = [-1, 0, 0, 1, 2, 2, 3, -2, 9]


# ------------------------------------------------------------------------------------------------ state generation
def _routing_image(lib, rng, cur=(0, 0), bad_golfer=None, long_list=False):
    img = bytearray(TC._panel_image(lib, rng))

    def p32(nm, v, at=0):
        TC.put32(lib, img, nm, v, at)
    o = TC.objs(lib)
    mode = rng.choice(MODES)
    if cur[0] >= 0x307 and rng.random() < 0.9:   # the cursor is over a scroll arrow: they exist only in mode 0
        mode = 0
    p32("g_ui_4d20dc", mode)
    p32("g_map822b80", 1 if rng.random() < 0.1 else 0)
    null_display = rng.random() < 0.05
    p32("g_holesPlusOne", rng.choice([0, 1, 5, 12, 19, 20]))
    # the display and the surface slot: a fake display, no surface
    struct.pack_into("<I", img, 0x00519CD8 - 0x004C1000, 0)
    struct.pack_into("<I", img, 0x0083AD50 - 0x004C1000, 0 if null_display else TC.FAKE_DISPLAY)
    # the cell sheets' frame counts: mostly small positive, a zero now and then (the walker row's modulo)
    soff, ssz = o["g_sheetCols"]
    zero_p = 0.0 if cur[0] >= 0x307 else rng.choice([0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.02, 0.4])
    cols = [0 if rng.random() < zero_p else rng.choice([1, 2, 3, 4, 8, 8, 8, 12, 16]) for _ in range(ssz // 4)]
    struct.pack_into(f"<{len(cols)}i", img, soff, *cols)
    # the walkers: the first nw are listed (flags set, a negative state), the others are not
    woff, _ = o["g_walkers"]
    nw = rng.choice([0, 3, 6, 8, 9, 10, 12, 20, 40, 64])
    if long_list or (mode == 0 and rng.random() < (0.9 if cur[0] >= 0x307 else 0.4)):   # a long list: the scroll arrows exist
        nw = rng.choice([9, 9, 10, 12, 20, 40, 64])
    for i in range(64):
        r = woff + 0x4C * i
        img[r:r + 0x4C] = rng.randbytes(0x4C)
        listed = i < nw
        img[r + 0x10] = rng.randrange(0, 50) if rng.random() < 0.9 else rng.randrange(256)
        img[r + 0x11] = rng.randrange(0, 50) if rng.random() < 0.9 else rng.randrange(256)
        if listed:
            img[r + 0x12] = rng.choice([1, 2, 8, 9, 0x10, 0x18, 0xFF])
            st = rng.choice([-1, -2, -3, -4, -5, -6, -6, -7, -8]) if rng.random() < 0.93 or cur[0] >= 0x307 else rng.randrange(-128, 0)
            img[r + 0x13] = st & 0xFF
        else:
            if rng.random() < 0.5:   # not listed: no flag byte, or a state that is not negative (or the skipped 0xfa)
                img[r + 0x12] = 0
                img[r + 0x13] = rng.randrange(256)
            else:
                img[r + 0x12] = rng.choice([1, 8, 0xFF])
                img[r + 0x13] = rng.choice([0, 1, 3, 0x7F, 0xFA])
        struct.pack_into("<h", img, r + 0x1C, rng.choice([0, 8, 16, 7, 24, rng.randrange(-300, 300)]))
        struct.pack_into("<h", img, r + 0x1E, rng.choice([0, 1, 5, 0xB, 0xC, 0xC, 0xC, 0xD, 0xE, rng.randrange(-5, 20)]))
        struct.pack_into("<hhh", img, r + 0x22, rng.randrange(-2000, 3000), rng.randrange(-50, 5000), rng.randrange(-5, 99))
    # the hole records 1..18 (0x208 bytes): par byte, tee and green tiles, the time fields the list divides
    hoff, _ = o["g_holes"]
    n = rng.choice([18, 18, 18, 9, 12, 0, 5, 1])
    for h in range(0, 20):
        r = hoff + 0x208 * h
        img[r:r + 0x208] = rng.randbytes(0x208) if h in (0, 19) or rng.random() < 0.2 else bytes(0x208)
        if 1 <= h <= 18:
            img[r] = (rng.choice([3, 4, 5, 4, 3, 5, -3, 7]) if h <= n else 0) & 0xFF
            for off in (8, 0xC, 0x18, 0x1C):
                v = rng.randrange(0, 50) if rng.random() < 0.95 else rng.choice([-1, 60, 4000, -2])
                struct.pack_into("<i", img, r + off, v)
            struct.pack_into("<i", img, r + 0x20, rng.choice([0, 0, 1, 10, 50, -1, -1]))
            struct.pack_into("<i", img, r + 0x1EC, rng.choice([0, 400, 12000, rng.randrange(-2 ** 31, 2 ** 31)]))
            struct.pack_into("<h", img, r + 4, rng.randrange(0, 600))
    for h in range(19):   # waypoint records: six dwords, -1 = none
        for k in range(6):
            struct.pack_into("<i", img, TC.objs(lib)["g_holePoints"][0] + 0x18 * h + 4 * k,
                             -1 if rng.random() < 0.4 else rng.randrange(0, 50 << 10))
    # the tile map: types (0x14 = no tile), the per-type 0xff table and the type records' class byte the colours switch on
    toff, _ = o["g_tileType"]
    img[toff:toff + 2500] = bytes(rng.choice([0x14, 0x14, 0, 1, 2, 3, 0xD, 0xE, 0xF, 0x10, 0x11, rng.randrange(23)]) for _ in range(2500))
    uoff, usz = o["unk_0058dd34"]
    img[uoff:uoff + usz] = bytes(rng.choice([0xFF, 0xFF, 0, 5]) for _ in range(usz))
    aoff, _ = o["g_tileTypeAttrs"]
    for t in range(23):
        img[aoff + 0x26 + 0x30 * t] = rng.choice([0, 1, 2, 4, 7, 10, 0xD, 0x11, 0x12, 0x13, rng.randrange(256)])
    TC.rand_obj(lib, img, "g_tileShadeA", rng)
    TC.rand_obj(lib, img, "g_tileShadeB", rng)
    TC.rand_obj(lib, img, "g_tileFlags", rng)
    # the golfers' hole bytes: 0 .. 19 in a real game; bytes of 20 or more read the caller's frame (U-1020)
    goff, _ = o["g_golfers"]
    for g in range(156):
        b = rng.choice([0, 0, -1, rng.randrange(1, 19), rng.randrange(1, 19), 19, rng.randrange(1, 19)])
        img[goff + 0x100 * g + GOLFERS_HOLE_BYTE] = b & 0xFF
    if bad_golfer is None:
        bad_golfer = rng.random() < 0.05
    if bad_golfer:
        img[goff + 0x100 * rng.randrange(152) + GOLFERS_HOLE_BYTE] = rng.choice([20, 21, 33, 127])
    if rng.random() < 0.8:   # a quiet input state: the loop must wait for what the script delivers
        for nm in ("g_rootInput", "gDirtyFlagA", "gDirtyFlagB", "g_redrawPending", "g_flag_83afcc"):
            TC.put(lib, img, nm, bytes(TC.objs(lib)[nm][1]))
    return bytes(img)


def _row_y(rng):
    """a y on one of the hole list's rows (17 px apart from 0x68), now and then above the first row or below the ninth"""
    return rng.choice([0x68 + 17 * rng.randrange(0, 10) + rng.randrange(0, 17), rng.randrange(0x60, 0x101)])


def _inputs(rng, cur=(0, 0)):
    from port.test import cmdemu as CE
    out = []
    if cur[0] >= 0x307:   # the cursor is over a scroll arrow: a few clicks on it scroll the list
        for _ in range(rng.choice([0, 1, 2, 3, 4])):
            out += CE.click(0x30D, cur[1])
    for _ in range(rng.choice([1, 1, 2, 3, 4, 6, 8])):
        k = rng.random()
        x = rng.choice([rng.randrange(0, 800), rng.randrange(0x190, 0x1A0), rng.randrange(0, 0x190), rng.randrange(0x190, 700),
                        rng.randrange(0x17C, 0x194)])
        if k < 0.3:    # a left click on the hole list
            out += CE.click(x, _row_y(rng))
        elif k < 0.5:  # a right click there: moves the selected hole
            out += [CE.Raw(4, x, _row_y(rng), rng.getrandbits(16), rng.getrandbits(16)), CE.Raw(3, 0, 0)]
        elif k < 0.55:  # select a hole, then ask to move it onto its own row
            y = _row_y(rng)
            out += CE.click(x, y) + [CE.Raw(4, x, y, 0, 0), CE.Raw(3, 0, 0)]
        elif k < 0.75:  # the button row (a few clicks right on the hovered button's rows)
            out += CE.click(x, rng.choice([rng.randrange(0x119, 0x260), rng.randrange(0x40, 0x65), rng.randrange(0xCD, 0xF2)]))
        elif k < 0.85:  # a key
            out += CE.key(rng.choice([0x1B, 0x41, 0x50, 0x0D]), rng.choice([0, 0x61, 0x1B]))
        elif k < 0.92:  # the strip between the map and the buttons: ends the screen
            out += CE.click(x, rng.randrange(0x101, 0x119))
        else:
            out.append(CE.Raw(rng.choice([2, 3, 9]), rng.randrange(800), rng.randrange(600)))
    e = rng.random()
    if e < 0.45:   # most runs end by themselves: a click on the strip under the map
        out += CE.click(rng.randrange(800), rng.randrange(0x101, 0x119))
    elif e < 0.7:  # or on a button row (the mode buttons set the mode, the quit button ends)
        out += CE.click(rng.choice([rng.randrange(800), 0xC0, 0x230, 0x2A0, 0x300]), rng.randrange(0x119, 0x260))
    return out


BUTTONS = [(0x3E, 0x13B), (0xB7, 0xFE), (0x218, 0xFE), (0x293, 0x13B), (0x296, 0x214), (0x307, 0x40), (0x307, 0xCD)]


def _cursor(rng):
    if rng.random() < 0.6:
        bx, by = rng.choice(BUTTONS)
        return bx + rng.randrange(-4, 0x28), by + rng.randrange(-6, 0x34)
    return rng.randrange(-10, 810), rng.randrange(-10, 610)


BOXES = [(0x3E, 0x8E, 0x13B, 0x18B), (0xB7, 0x107, 0xFE, 0x14E), (0x218, 0x268, 0xFE, 0x14E), (0x293, 0x2E3, 0x13B, 0x18B),
         (0x296, 0x2E6, 0x214, 0x255), (0x307, 0x319, 0x40, 0x65), (0x307, 0x319, 0xCD, 0xF2)]


def boundary_cases(lib):
    """the cursor on and around the edges of each of the seven hit boxes (mode 0, a long walker list so the arrows exist); a click on
    the button row then writes the hit state - 1 into the mode word (arrows 8 and 9), the quit box ends the screen, no box redraws
    and the strip click ends it"""
    from port.test import cmdemu as CE
    rng = random.Random(f"{NAME}/boundary")
    out = []
    for x0, x1, y0, y1 in BOXES:
        for x in (x0 - 1, x0, x1 - 1, x1):
            for y in (y0 - 1, y0, y1 - 1, y1):
                img = bytearray(_routing_image(lib, rng, (0x307, y), bad_golfer=False, long_list=True))
                TC.put32(lib, img, "g_ui_4d20dc", 0)
                TC.put32(lib, img, "g_map822b80", 0)
                for nm in ("g_rootInput", "gDirtyFlagA", "gDirtyFlagB", "g_redrawPending", "g_flag_83afcc"):
                    TC.put(lib, img, nm, bytes(TC.objs(lib)[nm][1]))
                inputs = CE.click(0x100, 0x120) + CE.click(0x100, 0x108)
                out.append((bytes(img), [], inputs, (x, y)))
    return out


def cases(lib, n, seed):
    return list(iter_cases(lib, n, seed))


def iter_cases(lib, n, seed, **kw):
    """the cases one at a time (an image is 3.6 MB: a long run must not hold them all)"""
    rng = random.Random(f"{NAME}/{seed}")
    for _ in range(n):
        cur = _cursor(rng)
        img = _routing_image(lib, rng, cur, **kw)
        yield (img, [], _inputs(rng, cur), cur)


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
    e.stub(TC.FAKE_DISPLAY + 0x820, 3, 0, True, "display slot 0x9c")   # the screen's call: three arguments, ret 0xc
    slots = [TC.FAKE_DISPLAY + 0x800] * 0x100
    slots[0x9C // 4] = TC.FAKE_DISPLAY + 0x820
    vt = struct.pack("<I", TC.FAKE_DISPLAY + 0x100) + bytes(0xFC) + struct.pack("<256I", *slots)
    block = (TC.FAKE_DISPLAY, vt.ljust(0x900, b"\xC3"))
    r = e.run(ENTRY, args, img, inputs=inputs, blocks=[block], cover=cover)
    gc.collect()   # a Unicorn instance and its hook closures form a cycle: free the 3.6 MB images now
    return r


def port(lib, img, args, inputs, cur):
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
        try:
            eax, code = st.call(NAME, list(args)).eax, 0
        except L.SgError as e:
            eax, code = None, e.code
        used = st.call("commands.inputConsumed", []).eax
        st.call("commands.clearInput", [])
        return eax, st.image(), code, used


def _bad_golfer(lib, img):
    off, _ = TC.objs(lib)["g_golfers"]
    return any(20 <= img[off + 0x100 * g + GOLFERS_HOLE_BYTE] < 128 for g in range(152))


def _drop_bad_bytes(lib, im):
    """OFF the defect: a hole byte of 20 or more is left alone by the port and garbage in the original: compare without them"""
    off, _ = TC.objs(lib)["g_golfers"]
    for g in range(152):
        if 20 <= im[off + 0x100 * g + GOLFERS_HOLE_BYTE] < 128:
            im[off + 0x100 * g + GOLFERS_HOLE_BYTE] = 0


def compare(lib, case, defect_off=False) -> str:
    img, args, inputs, cur = case
    r = emu(img, args, inputs, cur)
    pe, pimg, code, used = port(lib, img, args, inputs, cur)
    if r.err.startswith("NeedInput"):
        return "" if code == L.SG_E_NO_RESOURCE else f"original waits for input, port {code} {pe}"
    if r.err:
        return "" if pe is None and code in (L.SG_E_FAULT, L.SG_E_DIVIDE) else f"original error {r.err}, port {pe} {code}"
    if pe is None:
        # ON the defect the port reports the caller's frame as a missing resource where the original reads it
        if code == L.SG_E_NO_RESOURCE and _bad_golfer(lib, img) and not defect_off:
            return ""
        return f"port error {code}, original eax {r.eax:#x}"
    if used != r.consumed:
        return f"input consumed port {used} original {r.consumed}"
    if pe != r.eax:
        return f"eax port {pe:#x} original {r.eax:#x}"
    b = bytearray(pimg), bytearray(r.image)
    for im in b:
        for nm, at, ln in MASK:
            if nm in TC.objs(lib):
                off, size = TC.objs(lib)[nm]
                ln2 = size - at if ln is None else ln
                im[off + at:off + at + ln2] = bytes(ln2)
        if defect_off:
            _drop_bad_bytes(lib, im)
    return TC.first_diff(bytes(b[0]), bytes(b[1]), lib)


@need_exe
@emu_oracle
def test_routing_map_equals_emulated_original(portlib):
    bad = []
    for k, case in enumerate(iter_cases(portlib, int(os.environ.get("SG_ROUTING_N", "150")), 1)):
        d = compare(portlib, case)
        if d:
            bad.append((k, len(case[2]), case[3], d))
    assert not bad, f"{len(bad)} differ: {bad[:4]}"


@need_exe
@emu_oracle
def test_routing_map_defect_off_equals_original_without_frame_reads(portlib):
    """OFF exitRemapReadsFrame the port leaves a hole byte of 20 or more alone: every other byte of the image equals the original's"""
    portlib.defect("showRoutingMapScreen.exitRemapReadsFrame", False)
    try:
        bad = []
        for k, case in enumerate(iter_cases(portlib, int(os.environ.get("SG_ROUTING_N", "60")), 2, bad_golfer=True)):
            d = compare(portlib, case, defect_off=True)
            if d:
                bad.append((k, len(case[2]), case[3], d))
    finally:
        portlib.defect_reset()
    assert not bad, f"{len(bad)} differ: {bad[:4]}"


@need_exe
@emu_oracle
def test_hit_box_edges_equal_emulated_original(portlib):
    bad = []
    for k, case in enumerate(boundary_cases(portlib)):
        d = compare(portlib, case)
        if d:
            bad.append((k, case[3], d))
    assert not bad, f"{len(bad)} differ: {bad[:4]}"


# ------------------------------------------------------------------------------------------------ golden digests
GOLDEN_N = 120
NEGCTL_RED = [
    (1, "the reorder leaves the moved hole's tee tile number alone"),
    (2, "the exit remaps through the permutation, not its inverse"),
    (3, "the hole count after the exit is one more than the first hole with no par"),
    (4, "a button click sets the mode to the button, not the button - 1"),
    (5, "the walker scroll arrows are swapped"),
    (6, "the right half of the hole list starts at x >= 0x180"),
    (7, "the waypoint record does not move with its hole"),
    (8, "a listed walker whose kind is 0xc keeps it"),
    (9, "a walker's date is formatted from the word shifted by 9"),
]


def _golden_cases(lib):
    key = ("golden", NAME)
    if key not in TC._BASE:
        TC._BASE[key] = cases(lib, GOLDEN_N, "golden")
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


def test_every_routing_function_has_a_negctl(portlib):
    mine = {n for n, f in portlib.functions().items() if f.get("module") == "routing"}
    assert mine == {NAME, "showShortcutsScreen", "showMembershipRoster"}
    assert "showRoutingMapScreen.exitRemapReadsFrame" in portlib.defects()
