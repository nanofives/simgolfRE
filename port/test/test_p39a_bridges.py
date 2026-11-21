"""P39A1 play switch scenicBridge.twoTile (1 = the original; play profile 0; log/roadmap/P39/P39A1_notes.md).

The two-tile scenic bridge (port/src/sim/crossings.{h,cpp}): two orthogonal water (0x11) or stream (0x0a) tiles with land at both
ends along the axis. Building one gives each water tile the original's Scenic Bridge bits (flag word 0x120 | style), each stream
tile the path bit 0x20 only, and both the bridge's build code 0x93 with the one-tile price as the refund (setGrids); the pair goes
into the crossings table, which saveGame writes to a sidecar `<save>.sgx` next to the .sve / world file and loadGame reads back
(an entry whose tiles no longer hold the bridge is dropped). The bulldozer (demolishObject) on either tile removes the pair.

Evidence here (headless, the C ABI, the crossings.* test entries over a course from P38B's flat course):
- the fitting rules (both tiles open water / stream, land at both ends, the preferred axis first, the clicked tile first or second);
- the tiles' bits, codes and refunds after a build, and the table entry (axis, style, which tiles are streams);
- demolishObject on one tile clears both and pays both refunds back; the table empties;
- saveGame / loadGame round trips at 50 (.sve) and 101 (.sgw): the sidecar is written beside the save, the .sve / .sgw is the same
  file as with the switch at 1 (the original reads it unchanged), a fresh state at 0 gets the table back, at 1 it stays empty; a
  save with the table empty removes a stale sidecar; a sidecar entry whose tiles were changed is dropped;
- controls (negctl crossings): 1 the land test at the ends skipped, 2 the sidecar not read back, 3 the partner not demolished:
  each reads RED.

P39A4 (log/roadmap/P39/P39A4_notes.md): P39A1's ninth row item is replaced by a bridge length button beside the open bridge row;
at two tiles every style of the row builds the pair in that style. Judged here:
- the button: crossings.button is the hit test the press and the drawing share (only with the switch at 0 and the bridge row
  open); clickAmenityPanel on it flips the length and hands back the row's pick unchanged (its caller stores the result in
  unk_0056a514); with the switch at 1 the same press leaves the length alone;
- crossings.pairStyle (the rule H2_Build case 5 and the cursor preview use): a row style 0..7 builds a pair only at two tiles, a
  journalled pair (8 + style) always, nothing at 1;
- crossings.click (H2_Build case 5's play branch, the same function): every style 0..7 builds its pair in that style (the water
  tiles' low 3 flag bits, the table entry) and returns the charge 2 x the tool's price word (every style shares it); one tile
  answers -3 (the original's branch); unk_0056a514 is put back after the build;
- the sidecar keeps each pair's style (pairs of styles 2 and 6 in one save).
Controls: 8 (the length button ignored) and 9 (the pair built in the test entries' style) read RED.

The art of the button, the preview, the build through the live bridge tool, undo / redo are checked in the app drives
(log/roadmap/P39/P39A4_notes.md, screenshots).

    ~/bin/py -3.12 -m pytest port/test/test_p39a_bridges.py -q
"""
from __future__ import annotations

import struct

import pytest

from port.test.test_p38d_world_save import Names, _finish_load, _load, _save, _state
from port.test.test_terrain import need_exe, objs

pytestmark = pytest.mark.port

SWITCH = "scenicBridge.twoTile"
NEGCTL = "crossings.count"   # every crossings.* entry shares the module's negctl word
WATER, STREAM, ROUGH, FORSALE = 0x11, 0x0A, 0x04, 0x14
PATH, SCENIC = 1 << 5, 1 << 8
CODE = 0x93
PRICE = 100


@pytest.fixture(autouse=True)
def _reset(portlib):
    portlib.defect_reset()
    portlib.negctl_reset()
    yield
    portlib.defect_reset()
    portlib.negctl_reset()


def _n(st):
    return st.world_n()


def _set(st, grid, x, y, value, size=1):
    st.mem_write(st.grid_va(grid) + (x * _n(st) + y) * size, value.to_bytes(size, "little", signed=value < 0))


def _get(st, grid, x, y, size=1, signed=False):
    return int.from_bytes(st.mem_read(st.grid_va(grid) + (x * _n(st) + y) * size, size), "little", signed=signed)


def _paint(st, x0, y0, rows):
    """rows[i][j] is the type of tile (x0 + j, y0 + i): 'w' water, 's' stream, 'r' rough, 'o' for sale; flags 0, code 0xff"""
    kinds = {"w": WATER, "s": STREAM, "r": ROUGH, "o": FORSALE}
    for i, row in enumerate(rows):
        for j, ch in enumerate(row):
            x, y = x0 + j, y0 + i
            _set(st, "g_tileType", x, y, kinds[ch])
            _set(st, "g_tileFlags", x, y, 0, 2)
            _set(st, "g_tileBuildCode", x, y, 0xFF)
            _set(st, "g_tileRefund", x, y, 0)


def _unpack(v):
    if v == 0xFFFFFFFF:
        return None
    return {"x": v & 0xFF, "y": (v >> 8) & 0xFF, "axis": (v >> 16) & 0xF, "style": (v >> 20) & 0xF, "kind": (v >> 24) & 0xFF}


def _fit(st, x, y, axis):
    return _unpack(st.call("crossings.fit", [x, y, axis]).eax)


def _table(st):
    return [_unpack(st.call("crossings.get", [i]).eax) for i in range(st.call("crossings.count", []).eax)]


def _cash(portlib, st):
    o = objs(portlib)
    return struct.unpack_from("<i", st.image(), o["g_cash_div100"][0])[0]


# the test course: at (20, 20) a row of rough, two water tiles, rough (along x); at (20, 24) a column rough, stream, stream, rough
# (along y); at (30, 20) water, water, water with rough at both ends (3 wide: two bridges fit, not one over all three); at
# (30, 24) rough, water, water, for sale (an end that is not course land)
def _course(st):
    _paint(st, 20, 20, ["rwwr"])
    _paint(st, 20, 24, ["r", "s", "s", "r"])
    _paint(st, 30, 20, ["rwwwr"])
    _paint(st, 30, 24, ["rwwo"])


@pytest.fixture
def course(portlib):
    st = _state(portlib, 50)
    try:
        _course(st)
        yield st
    finally:
        st.__exit__(None, None, None)


@need_exe
def test_fitting_rules(portlib, course):
    st = course
    # water along x: the clicked tile first, or second when the first does not fit
    assert _fit(st, 21, 20, 0) == {"x": 21, "y": 20, "axis": 0, "style": 0, "kind": 0}
    assert _fit(st, 22, 20, 0) == {"x": 21, "y": 20, "axis": 0, "style": 0, "kind": 0}
    # the preferred axis does not fit (water ends along y): the other axis
    assert _fit(st, 21, 20, 1)["axis"] == 0
    # stream along y, both tiles streams (kind bits 0 and 1)
    assert _fit(st, 20, 25, 1) == {"x": 20, "y": 25, "axis": 1, "style": 0, "kind": 3}
    assert _fit(st, 20, 26, 0) == {"x": 20, "y": 25, "axis": 1, "style": 0, "kind": 3}
    # three water tiles: a pair needs land at both ends, so the middle tile fits with neither neighbour
    assert _fit(st, 31, 20, 0) is None and _fit(st, 32, 20, 0) is None and _fit(st, 33, 20, 0) is None
    # an end for sale (0x14) is not land
    assert _fit(st, 31, 24, 0) is None
    # rough is not a bridge tile
    assert _fit(st, 20, 20, 0) is None
    # crossing_fit gives the test entries' style (H2_Build sets the clicked item's style on the pair: test_click_builds_every_style)
    st.call("crossings.setStyle", [5])
    assert _fit(st, 21, 20, 0)["style"] == 5


@need_exe
def test_build_bits_and_table(portlib, course):
    st = course
    st.call("crossings.setStyle", [3])
    assert st.call("crossings.build", [22, 20, 0, PRICE]).eax == 1
    assert st.call("crossings.build", [20, 26, 1, PRICE]).eax == 1
    for x, y in ((21, 20), (22, 20)):   # water: the original's Scenic Bridge bits
        assert _get(st, "g_tileFlags", x, y, 2) == SCENIC | PATH | 3
        assert _get(st, "g_tileBuildCode", x, y) == CODE and _get(st, "g_tileRefund", x, y, signed=True) == PRICE
    for x, y in ((20, 25), (20, 26)):   # stream: the path bit only
        assert _get(st, "g_tileFlags", x, y, 2) == PATH
        assert _get(st, "g_tileBuildCode", x, y) == CODE
    assert _table(st) == [{"x": 21, "y": 20, "axis": 0, "style": 3, "kind": 0}, {"x": 20, "y": 25, "axis": 1, "style": 3, "kind": 3}]
    assert st.call("crossings.at", [22, 20]).eax == 0 and st.call("crossings.at", [20, 25]).eax == 1
    # a built tile is no longer open: nothing fits over it
    assert _fit(st, 21, 20, 0) is None


@need_exe
@pytest.mark.parametrize("mode", [0, 3])
def test_bulldozer_removes_the_pair(portlib, course, mode):
    st = course
    portlib.defect(SWITCH, False)
    st.call("crossings.build", [21, 20, 0, PRICE])
    cash = _cash(portlib, st)
    portlib.negctl(NEGCTL, mode)
    st.call("demolishObject", [22, 20, 0])
    ok = (st.call("crossings.count", []).eax == 0
          and all(_get(st, "g_tileFlags", x, 20, 2) & PATH == 0 for x in (21, 22))
          and all(_get(st, "g_tileBuildCode", x, 20) == 0xFF for x in (21, 22))
          and _cash(portlib, st) == cash + 2 * PRICE)
    assert ok == (mode == 0), f"mode {mode}: the pair {'kept a tile' if mode == 0 else 'went whole under the control'}"


@need_exe
def test_bulldozer_at_1_is_the_original(portlib, course):
    """with the switch at 1 the table is not consulted: a tile built through the test entry is demolished alone"""
    st = course
    st.call("crossings.build", [21, 20, 0, PRICE])
    st.call("demolishObject", [22, 20, 0])
    assert _get(st, "g_tileBuildCode", 21, 20) == CODE and _get(st, "g_tileBuildCode", 22, 20) == 0xFF


def _saved(st, stem, ext):
    return st.files_read(f"Saved Games/{stem}{ext}"), st.files_read(f"Saved Games/{stem}.sgx")


def _build_and_save(portlib, n, at, stem="Br"):
    st = _state(portlib, n)
    names = Names(st)
    try:
        portlib.defect(SWITCH, bool(at))
        _course(st)
        st.call("crossings.setStyle", [6])
        st.call("crossings.build", [21, 20, 0, PRICE])
        st.call("crossings.build", [20, 25, 1, PRICE])
        assert _save(st, names, stem) == 0
        return _saved(st, stem, ".sve" if n == 50 else ".sgw")
    finally:
        st.__exit__(None, None, None)


def _load_table(portlib, n, at, raw, side, stem="Br", negctl=0):
    st = _state(portlib, n)
    names = Names(st)
    try:
        portlib.defect(SWITCH, bool(at))
        ext = ".sve" if n == 50 else ".sgw"
        st.files_put(f"Saved Games/{stem}{ext}", raw)
        if side is not None:
            st.files_put(f"Saved Games/{stem}.sgx", side)
        portlib.negctl(NEGCTL, negctl)
        _finish_load(st, names, stem + ".sve")
        portlib.negctl(NEGCTL, 0)
        return _table(st)
    finally:
        st.__exit__(None, None, None)


EXPECT = [{"x": 21, "y": 20, "axis": 0, "style": 6, "kind": 0}, {"x": 20, "y": 25, "axis": 1, "style": 6, "kind": 3}]


@need_exe
@pytest.mark.parametrize("n", [50, 101])
def test_sidecar_round_trip(portlib, n):
    raw0, side0 = _build_and_save(portlib, n, 0)
    raw1, side1 = _build_and_save(portlib, n, 1)
    assert raw0 is not None and side0 is not None
    assert side1 is None, "the switch at 1 wrote a sidecar"
    # the save itself does not depend on the switch (the original reads it as before); the header's bytes after its NUL hold the
    # buffer's earlier contents (the save path), so the comparison skips them as test_p38d_world_save does
    h = 0 if n == 50 else 0x28
    assert len(raw0) == len(raw1) and raw0[:h] == raw1[:h] and raw0[h + 0x64:] == raw1[h + 0x64:]
    # P39A3: version 2, 12 bytes an entry (type, span, deck, extra after the P39A1 fields)
    assert side0[:4] == b"SGXB" and struct.unpack_from("<III", side0, 4) == (2, n, 2) and len(side0) == 16 + 2 * 12
    assert _load_table(portlib, n, 0, raw0, side0) == EXPECT
    assert _load_table(portlib, n, 1, raw0, side0) == [], "the switch at 1 read the sidecar"
    assert _load_table(portlib, n, 0, raw0, None) == []
    assert _load_table(portlib, n, 0, raw0, side0, negctl=2) == [], "control 2 (the sidecar not read) still read it"


@need_exe
def test_sidecar_drops_changed_tiles(portlib):
    """an entry whose tiles no longer hold the bridge (code 0x93 and the path bit) is dropped at the load"""
    raw, side = _build_and_save(portlib, 50, 0)
    st = _state(portlib, 50)
    names = Names(st)
    try:
        portlib.defect(SWITCH, False)
        st.files_put("Saved Games/Br.sve", raw)
        st.files_put("Saved Games/Br.sgx", side)
        _finish_load(st, names, "Br.sve")
        assert len(_table(st)) == 2
        st.call("demolishObject", [20, 26, 0])   # the stream bridge goes (both tiles)
        _set(st, "g_tileBuildCode", 22, 20, 0xFF)   # the water bridge's second tile loses its code behind the table's back
        assert _save(st, names, "Br2") == 0
        _load(st, names, "Br2.sve")   # a browser preview's load (finish 0) reads the sidecar too
        assert _table(st) == []
        # malformed sidecars empty the table
        for bad in (side[:15], b"XXXX" + side[4:], side[:8] + struct.pack("<I", 77) + side[12:], side + b"\0"):
            st.files_put("Saved Games/Br.sgx", bad)
            _load(st, names, "Br.sve")
            assert _table(st) == []
    finally:
        st.__exit__(None, None, None)


@need_exe
def test_empty_table_removes_a_stale_sidecar(portlib):
    st = _state(portlib, 50)
    names = Names(st)
    try:
        portlib.defect(SWITCH, False)
        _course(st)
        st.call("crossings.build", [21, 20, 0, PRICE])
        assert _save(st, names, "Br") == 0
        assert st.files_read("Saved Games/Br.sgx") is not None
        st.call("demolishObject", [21, 20, 0])
        assert _save(st, names, "Br") == 0
        assert st.files_read("Saved Games/Br.sgx") is None
    finally:
        st.__exit__(None, None, None)


@need_exe
@pytest.mark.parametrize("mode", [0, 1])
def test_control_land_ends(portlib, course, mode):
    """control 1 (the land test at the ends skipped) fits a pair over three water tiles and over an end for sale"""
    st = course
    portlib.negctl(NEGCTL, mode)
    fits = _fit(st, 31, 20, 0) is not None or _fit(st, 31, 24, 0) is not None
    portlib.negctl(NEGCTL, 0)
    assert fits == (mode == 1)


# ---------------------------------------------------------------------------------------------------------------- P39A4
BUTTON = (0x2DF, 0x208)       # crossings.h kLengthButtonX / Y (800 space)
LAST_ITEM = (0x282, 0x20F)    # the row's last style (drawAmenityRow's point of item 7 of 8)
OLD_NINTH = (0x299, 0x1F4)    # P39A1's ninth item


def _poke(portlib, st, name, fmt, value, at=0):
    o = objs(portlib)
    img = bytearray(st.image())
    struct.pack_into(fmt, img, o[name][0] + at, value)
    st.load_image(bytes(img))


def _peek(portlib, st, name, fmt, at=0):
    return struct.unpack_from(fmt, st.image(), objs(portlib)[name][0] + at)[0]


def _s32(v):
    return v - (1 << 32) if v & 0x80000000 else v


def _bridge_row(portlib, st, pick=5):
    """the amenities palette open on the bridge row with style `pick` picked (f_w_4c2848[3] = 0x13, unk_0056a514 = the pick)"""
    _poke(portlib, st, "f_w_4c2848", "<i", 0x13, 12)
    _poke(portlib, st, "unk_0056a514", "<i", pick)
    _poke(portlib, st, "g_buildPanelSlot", "<i", 1)


@need_exe
def test_length_button_hit_and_press(portlib, course):
    st = course
    portlib.defect(SWITCH, False)
    _bridge_row(portlib, st)
    hit = lambda x, y: st.call("crossings.button", [x, y]).eax  # noqa: E731
    assert hit(*BUTTON) == 1 and hit(BUTTON[0] + 8, BUTTON[1] + 4) == 1
    assert hit(*LAST_ITEM) == 0 and hit(*OLD_NINTH) == 0 and hit(BUTTON[0], BUTTON[1] + 0x14) == 0
    assert st.call("crossings.twoMode", [2]).eax == 0, "a session starts at one tile"
    for expect in (1, 0, 1):
        r = _s32(st.call("clickAmenityPanel", list(BUTTON)).eax)
        assert r == 5, "the press changed the row's pick"
        assert st.call("crossings.twoMode", [2]).eax == expect
        assert _peek(portlib, st, "unk_0056a514", "<i") == 5 and _peek(portlib, st, "f_w_4c2848", "<i", 12) == 0x13
    # another row open (benches, tool 1): no button
    _poke(portlib, st, "f_w_4c2848", "<i", 1, 12)
    assert hit(*BUTTON) == 0
    # the switch at 1: no button, the press leaves the length alone
    portlib.defect(SWITCH, True)
    _bridge_row(portlib, st)
    assert hit(*BUTTON) == 0
    st.call("clickAmenityPanel", list(BUTTON))
    assert st.call("crossings.twoMode", [2]).eax == 1


@need_exe
@pytest.mark.parametrize("mode", [0, 8])
def test_pair_style_rule(portlib, course, mode):
    st = course
    portlib.defect(SWITCH, False)
    ps = lambda v: _s32(st.call("crossings.pairStyle", [v]).eax)  # noqa: E731
    portlib.negctl(NEGCTL, mode)
    st.call("crossings.twoMode", [0])
    one = [ps(v) for v in range(16)]
    st.call("crossings.twoMode", [1])
    two = [ps(v) for v in range(16)]
    portlib.negctl(NEGCTL, 0)
    ok = one == [-1] * 8 + list(range(8)) and two == list(range(8)) * 2 and ps(-1) == -1 and ps(16) == -1
    assert ok == (mode == 0), f"control {mode}: two tiles {two}"
    if mode == 0:
        portlib.defect(SWITCH, True)
        assert [ps(v) for v in range(16)] == [-1] * 16


# eight water pairs along x, two rows apart: (41, 10 + 2k), (42, 10 + 2k) with rough at both ends
def _rows(st):
    for k in range(8):
        _paint(st, 40, 10 + 2 * k, ["rwwr"])


@need_exe
@pytest.mark.parametrize("mode", [0, 9])
def test_click_builds_every_style(portlib, course, mode):
    st = course
    portlib.defect(SWITCH, False)
    _rows(st)
    st.call("crossings.setStyle", [0])
    st.call("crossings.twoMode", [1])
    _poke(portlib, st, "g_cash_div100", "<i", 100000)
    _poke(portlib, st, "unk_0056a514", "<i", 3)
    portlib.negctl(NEGCTL, mode)
    charges, styles, bits = [], [], []
    for s in range(8):
        charges.append(_s32(st.call("crossings.click", [42, 10 + 2 * s, s, PRICE]).eax))
        e = _table(st)[-1]
        styles.append(e["style"])
        bits.append([_get(st, "g_tileFlags", x, 10 + 2 * s, 2) & 7 for x in (41, 42)])
    portlib.negctl(NEGCTL, 0)
    assert charges == [2 * PRICE] * 8, "every style costs twice the bridge tool's price word"
    assert _peek(portlib, st, "unk_0056a514", "<i") == 3, "the build left the journal's variant in unk_0056a514"
    ok = styles == list(range(8)) and bits == [[s, s] for s in range(8)]
    assert ok == (mode == 0), f"control {mode}: styles {styles}"


@need_exe
def test_click_one_tile_and_refusals(portlib, course):
    st = course
    portlib.defect(SWITCH, False)
    _poke(portlib, st, "g_cash_div100", "<i", 100000)
    st.call("crossings.twoMode", [0])
    assert _s32(st.call("crossings.click", [21, 20, 4, PRICE]).eax) == -3, "one tile is the original's branch"
    st.call("crossings.twoMode", [1])
    assert _s32(st.call("crossings.click", [20, 20, 4, PRICE]).eax) == -1, "rough fitted a pair"
    # canAfford's refusal (cash below the charge on a course of 3+ holes) opens the game's modal window and waits, so it is not
    # driven headless; its branch is H2_Build's goto as in P39A1. The charge at exactly the cash is afforded.
    assert st.call("crossings.count", []).eax == 0 and _get(st, "g_tileBuildCode", 21, 20) == 0xFF
    _poke(portlib, st, "g_cash_div100", "<i", 2 * PRICE)
    assert _s32(st.call("crossings.click", [21, 20, 4, PRICE]).eax) == 2 * PRICE
    # a journalled pair (8 + style) builds at one tile too (the redo)
    st.call("crossings.twoMode", [0])
    _poke(portlib, st, "g_cash_div100", "<i", 100000)
    assert _s32(st.call("crossings.click", [20, 25, 8 + 7, PRICE]).eax) == 2 * PRICE
    assert [e["style"] for e in _table(st)] == [4, 7]


@need_exe
def test_sidecar_keeps_each_style(portlib):
    st = _state(portlib, 50)
    names = Names(st)
    try:
        portlib.defect(SWITCH, False)
        _course(st)
        _poke(portlib, st, "g_cash_div100", "<i", 100000)
        st.call("crossings.twoMode", [1])
        st.call("crossings.click", [21, 20, 2, PRICE])
        st.call("crossings.click", [20, 25, 6, PRICE])
        assert _save(st, names, "Styles") == 0
        raw, side = _saved(st, "Styles", ".sve")
    finally:
        st.__exit__(None, None, None)
    assert side[:4] == b"SGXB" and struct.unpack_from("<III", side, 4) == (2, 50, 2)
    assert [side[16 + 12 * k + 5] for k in range(2)] == [2, 6]
    got = _load_table(portlib, 50, 0, raw, side, stem="Styles")
    assert [(e["x"], e["y"], e["style"]) for e in got] == [(21, 20, 2), (20, 25, 6)]
