"""P39A3 play switch layPath.gapBridge (1 = the original; play profile 0; log/roadmap/P39/P39A3_notes.md).

Gap bridges (port/src/sim/crossings.{h,cpp}): the Pathway (object tool 0) or the path terrain tool 0xa0, laying a path beside or into a
gap of 1 or 2 tiles whose lowest corner (cornerHeights) lies at least 3 levels below both banks' facing edges (the lower of the bank's
two corners on that side; Mariano, round 2: the lowest corner, so dry dips count), with a path on the far bank, asks first (the game's yes / no popup) and on yes builds a bridge: the gap tiles
get the path bit and the bridge code 0x93 with a refund of min(price / span, 127), the rest of the price in the entry's `extra`; the
price is twice the Scenic Bridge's word plus 10 per level of depth (deck - the gap's lowest corner) per tile; the deck level is the
lower bank edge, and drawing raises a walker on the deck by the deck's height minus the ground's.

Neighbouring tiles share their corner vertices (a tile's corners 1 / 3 / 5 / 7 are the vertices (x+1, y-1), (x+1, y), (x, y),
(x, y-1)): a 2-tile dip has a vertex line of its own between its tiles, so a dry valley whose middle line lies 3 below the banks is a
gap; a 1-tile dip has no corner of its own (all four are the banks' edge vertices), so on unflattened land it never is one; a flattened
type is (water and pot bunker take their lowest corner, TTA_LEVEL_MIN, tees their highest, TTA_LEVEL_MAX). The courses here (P38B's
flat course at levels 8, edited):
- RAVINE: rough banks (20, 20) and (23, 20), water at (21, 20) / (22, 20), the vertex line x = 22 between them lowered to 5: the water
  tiles sit at 5, the bank edges at 8, so a span-2 gap 3 deep (the far bank (23, 20) a path);
- TEES: a water tile (31, 20) between tees (30, 20) / (32, 20) whose far vertices are raised to 11: a span-1 gap (tee edges 11, water 8);
- the same RAVINE dry (rough, not water): a dry valley, a gap of its own.

Evidence: detection (both spans, from the bank and from a gap tile, axis, deck), the thresholds (3 levels yes, 2 no; no far path, dry
ground or 3 tiles wide: no bridge), the question (no: nothing built and not asked again for that gap; yes: built and charged), the
price, the tiles' bits / codes / refunds / extra, the bulldozer (the whole bridge, every unit back), the deck lift for walkers, the
sidecar (version 2 round trip, version 1 still read), the switch at 1 (nothing), the steep-slope thought 0x2b skipped on a deck (one
updateGolfers call on mood5_fixture/00, as test_p39a_stairs). Controls (negctl crossings): 4 a gap 2 deep accepted, 5 the extra not
paid back, 6 the answer ignored, 7 no deck lift: each RED.

    ~/bin/py -3.12 -m pytest port/test/test_p39a_gap.py -q
"""
from __future__ import annotations

import struct

import pytest

from port.test.test_p38d_world_save import Names, _load, _save, _state
from port.test.test_terrain import need_exe, objs

pytestmark = pytest.mark.port

SWITCH = "layPath.gapBridge"
NEGCTL = "crossings.count"
WATER, ROUGH, TEE = 0x11, 0x04, 0x00
PATH = 1 << 5
CODE = 0x93
LEVEL = 8
WORD = 100   # the Scenic Bridge's price word (f_tab_buildItems_4c26b0 + 0x12 of tool 0x13)


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


def _vertex(st, vx, vy, level):
    st.mem_write(st.grid_va("g_cornerLevel") + vx * (_n(st) + 1) + vy, bytes([level]))


def _poke(portlib, st, name, fmt, value, at=0):
    o = objs(portlib)
    img = bytearray(st.image())
    struct.pack_into(fmt, img, o[name][0] + at, value)
    st.load_image(bytes(img))


def _tile(st, x, y, t, path=False):
    _set(st, "g_tileType", x, y, t)
    _set(st, "g_tileFlags", x, y, PATH if path else 0, 2)
    _set(st, "g_tileBuildCode", x, y, 0xFF)
    _set(st, "g_tileRefund", x, y, 0)


def _course(portlib, st, ravine_depth=3, dry=False, far_path=True, wide=False):
    n = _n(st)
    # the date (heightBlend reads g_cornerLevel once g_date != 0), the height scale and zoom the lift is measured in, cash
    _poke(portlib, st, "g_date", "<i", 0x2ABCD)
    _poke(portlib, st, "f_w_4c2e00", "<i", 3)
    _poke(portlib, st, "g_zoomScale", "<i", 4)
    _poke(portlib, st, "g_cash_div100", "<i", 5000)
    _poke(portlib, st, "g_cameraRot", "<i", 0)
    st.mem_write(st.grid_va("g_cornerLevel"), bytes([LEVEL]) * ((n + 1) * (n + 1)))
    for x in range(18, 40):
        for y in range(17, 24):
            _tile(st, x, y, ROUGH)
    # RAVINE
    gap = [21, 22, 23] if wide else [21, 22]
    for x in gap:
        _tile(st, x, 20, ROUGH if dry else WATER)
    far = gap[-1] + 1
    _tile(st, far, 20, ROUGH, path=far_path)
    for vx in range(gap[0] + 1, gap[-1] + 1):   # the vertex lines between the gap tiles
        for vy in (19, 20):
            _vertex(st, vx, vy, LEVEL - ravine_depth)
    # TEES: tee banks raised by their far vertices (TTA_LEVEL_MAX takes the max of the tile's corner range)
    _tile(st, 30, 20, TEE)
    _tile(st, 31, 20, WATER)
    _tile(st, 32, 20, TEE, path=True)
    for vy in (19, 20):
        _vertex(st, 30, vy, 11)
        _vertex(st, 33, vy, 11)
    st.mem_write(st.grid_va("g_cornerCache"), bytes(8 * n * n))   # no stale cached corner


@pytest.fixture
def course(portlib):
    st = _state(portlib, 50)
    try:
        _course(portlib, st)
        portlib.defect(SWITCH, False)
        yield st
    finally:
        st.__exit__(None, None, None)


def _unpack(v):
    if v == 0xFFFFFFFF:
        return None
    return {"x": v & 0xFF, "y": (v >> 8) & 0xFF, "axis": (v >> 16) & 1, "span": (v >> 20) & 0xF, "type": (v >> 24) & 3,
            "deck": (v >> 26) & 0x3F}


def _find(st, x, y):
    return _unpack(st.call("crossings.gapFind", [x, y]).eax)


def _cash(portlib, st):
    return struct.unpack_from("<i", st.image(), objs(portlib)["g_cash_div100"][0])[0]


RAVINE = {"x": 21, "y": 20, "axis": 0, "span": 2, "type": 1, "deck": LEVEL}
TEES = {"x": 31, "y": 20, "axis": 0, "span": 1, "type": 1, "deck": 11}


@need_exe
def test_heights_of_the_courses(course):
    st = course
    assert st.call("crossings.edge", [20, 20, 0]).eax == LEVEL       # the bank's +x edge
    assert st.call("crossings.edge", [23, 20, 1]).eax == LEVEL       # the far bank's -x edge
    assert st.call("crossings.top", [21, 20]).eax == LEVEL - 3       # water: its lowest corner, all four
    assert st.call("crossings.bottom", [22, 20]).eax == LEVEL - 3
    assert st.call("crossings.edge", [30, 20, 0]).eax == 11          # a tee: its highest corner
    assert st.call("crossings.top", [31, 20]).eax == LEVEL


@need_exe
def test_detection(course):
    st = course
    for x in (20, 21, 22):   # from the bank being laid, from either gap tile
        assert _find(st, x, 20) == RAVINE, x
    assert _find(st, 30, 20) == TEES   # the tee bank (30, 20): its far bank (32, 20) has a path
    assert _find(st, 31, 20) == TEES
    assert _find(st, 25, 20) is None   # no gap there


@need_exe
@pytest.mark.parametrize("case,expect", [
    ({"ravine_depth": 3}, RAVINE), ({"ravine_depth": 2}, None), ({"ravine_depth": 5}, RAVINE),
    ({"dry": True}, RAVINE), ({"dry": True, "ravine_depth": 2}, None), ({"dry": True, "ravine_depth": 4}, RAVINE),
    ({"far_path": False}, None), ({"wide": True}, None), ({"wide": True, "dry": True}, None)])
def test_thresholds(portlib, case, expect):
    st = _state(portlib, 50)
    try:
        _course(portlib, st, **case)
        portlib.defect(SWITCH, False)
        assert _find(st, 20, 20) == expect, case
    finally:
        st.__exit__(None, None, None)


@need_exe
@pytest.mark.parametrize("mode", [0, 4])
def test_control_threshold(portlib, mode):
    st = _state(portlib, 50)
    try:
        _course(portlib, st, ravine_depth=2)
        portlib.defect(SWITCH, False)
        portlib.negctl(NEGCTL, mode)
        found = _find(st, 20, 20) is not None
        portlib.negctl(NEGCTL, 0)
        assert found == (mode == 4)
    finally:
        st.__exit__(None, None, None)


@need_exe
def test_price(course):
    st = course
    assert st.call("crossings.gapPrice", [20, 20]).eax == 2 * WORD + 10 * 3 * 2   # depth 3, span 2: $26,000
    assert st.call("crossings.gapPrice", [31, 20]).eax == 2 * WORD + 10 * 3 * 1   # deck 11 over water at 8: $23,000


@need_exe
@pytest.mark.parametrize("mode", [0, 6])
def test_question_no(portlib, course, mode):
    st = course
    portlib.negctl(NEGCTL, mode)
    st.call("crossings.gapAnswer", [1])
    cash = _cash(portlib, st)
    r = st.call("crossings.gapTry", [20, 20]).eax
    built = st.call("crossings.count", []).eax
    portlib.negctl(NEGCTL, 0)
    if mode == 6:
        assert built == 1, "control 6 (the answer ignored) did not build"
        return
    assert r == 0 and built == 0 and _cash(portlib, st) == cash
    assert all(_get(st, "g_tileFlags", x, 20, 2) & PATH == 0 for x in (21, 22))
    # declined: the same gap is not asked again (a yes now still builds nothing) until another gap is asked
    st.call("crossings.gapAnswer", [0])
    assert st.call("crossings.gapTry", [21, 20]).eax == 0 and st.call("crossings.count", []).eax == 0
    assert st.call("crossings.gapTry", [31, 20]).eax == 2   # another gap: asked, yes
    assert st.call("crossings.gapTry", [20, 20]).eax == 1   # the first gap asked again


@need_exe
def test_question_yes_builds_and_charges(portlib, course):
    st = course
    st.call("crossings.gapAnswer", [0])
    cash = _cash(portlib, st)
    assert st.call("crossings.gapTry", [20, 20]).eax == 1   # from the bank: the bank's own path is laid after
    price = 2 * WORD + 60
    assert _cash(portlib, st) == cash - price
    assert _unpack(st.call("crossings.getGap", [0]).eax) == RAVINE
    for x in (21, 22):
        assert _get(st, "g_tileFlags", x, 20, 2) & PATH
        assert _get(st, "g_tileFlags", x, 20, 2) & (1 << 8) == 0   # no scenic bit: no scenic thought
        assert _get(st, "g_tileBuildCode", x, 20) == CODE
        assert _get(st, "g_tileRefund", x, 20, signed=True) == 127
    assert st.call("crossings.extra", [0]).eax == price - 2 * 127
    # from a gap tile: the bridge holds it (2)
    assert st.call("crossings.gapTry", [31, 20]).eax == 2
    assert st.call("crossings.extra", [1]).eax == (2 * WORD + 30) - 127


@need_exe
@pytest.mark.parametrize("mode", [0, 5])
def test_bulldozer_pays_everything_back(portlib, course, mode):
    st = course
    st.call("crossings.gapAnswer", [0])
    cash = _cash(portlib, st)
    st.call("crossings.gapTry", [20, 20])
    portlib.negctl(NEGCTL, mode)
    st.call("demolishObject", [22, 20, 0])
    portlib.negctl(NEGCTL, 0)
    gone = st.call("crossings.count", []).eax == 0 and all(_get(st, "g_tileFlags", x, 20, 2) & PATH == 0 for x in (21, 22))
    assert gone
    assert (_cash(portlib, st) == cash) == (mode == 0), f"mode {mode}: cash {cash} -> {_cash(portlib, st)}"


@need_exe
@pytest.mark.parametrize("mode", [0, 7])
def test_deck_lift(portlib, course, mode):
    st = course
    st.call("crossings.gapAnswer", [0])
    st.call("crossings.gapTry", [20, 20])
    portlib.negctl(NEGCTL, mode)
    c = ((21 << 10) + 0x200, (20 << 10) + 0x200)
    lift = st.call("crossings.deckLift", list(c)).eax
    off = st.call("crossings.deckLift", [(20 << 10) + 0x200, (20 << 10) + 0x200]).eax
    portlib.negctl(NEGCTL, 0)
    deck_px = st.call("crossings.levelPx", [LEVEL]).eax
    ground = st.call("crossings.groundPx", list(c)).eax
    assert deck_px == (LEVEL - 3) * 3 * 4 // 4 and ground == (LEVEL - 3 - 3) * 3 * 4 // 4
    assert off == 0
    assert (lift == deck_px - ground == 9) == (mode == 0), f"mode {mode}: lift {lift}"


@need_exe
def test_switch_at_1_builds_nothing(portlib):
    st = _state(portlib, 50)
    try:
        _course(portlib, st)
        st.call("crossings.gapAnswer", [0])
        assert st.call("crossings.gapTry", [20, 20]).eax == 0 and st.call("crossings.count", []).eax == 0
        assert st.call("crossings.deckLift", [(21 << 10) + 0x200, (20 << 10) + 0x200]).eax == 0
    finally:
        st.__exit__(None, None, None)


@need_exe
def test_sidecar_round_trip(portlib):
    st = _state(portlib, 50)
    names = Names(st)
    try:
        _course(portlib, st)
        portlib.defect(SWITCH, False)
        st.call("crossings.gapAnswer", [0])
        st.call("crossings.gapTry", [20, 20])
        st.call("crossings.gapTry", [31, 20])
        want = [_unpack(st.call("crossings.getGap", [i]).eax) for i in range(2)]
        extras = [st.call("crossings.extra", [i]).eax for i in range(2)]
        assert _save(st, names, "Gap") == 0
        side = st.files_read("Saved Games/Gap.sgx")
        assert side[:4] == b"SGXB" and struct.unpack_from("<III", side, 4) == (2, 50, 2) and len(side) == 16 + 2 * 12
        st.call("crossings.clear", [])
        _load(st, names, "Gap.sve")
        assert [_unpack(st.call("crossings.getGap", [i]).eax) for i in range(2)] == want
        assert [st.call("crossings.extra", [i]).eax for i in range(2)] == extras
        # a version 1 sidecar (P39A1, 8 bytes an entry) is still read: its entries are two-tile bridges (here dropped, since the
        # tiles hold a gap bridge's span, not a pair; the file is accepted, not refused as malformed)
        v1 = b"SGXB" + struct.pack("<III", 1, 50, 1) + struct.pack("<HHBBBB", 21, 20, 0, 0, 0, 0)
        st.files_put("Saved Games/Gap.sgx", v1)
        _load(st, names, "Gap.sve")
        assert st.call("crossings.count", []).eax == 1   # tiles (21, 20) and (22, 20) carry the path bit and code 0x93
        assert _unpack(st.call("crossings.getGap", [0]).eax)["type"] == 0
    finally:
        st.__exit__(None, None, None)


@need_exe
def test_dry_valley_deck_and_lift(portlib):
    """a dry valley (rough, the middle vertex line 3 below): built after the yes; the deck lift at a gap tile's centre is the deck level's
    height minus the slope's there (positive); no stairs on the deck tiles"""
    st = _state(portlib, 50)
    try:
        _course(portlib, st, dry=True)
        portlib.defect(SWITCH, False)
        portlib.defect("layPath.stairs", False)
        st.call("crossings.gapAnswer", [0])
        assert st.call("crossings.gapTry", [20, 20]).eax == 1
        c = ((21 << 10) + 0x200, (20 << 10) + 0x200)
        lift = st.call("crossings.deckLift", list(c)).eax
        assert lift == st.call("crossings.levelPx", [LEVEL]).eax - st.call("crossings.groundPx", list(c)).eax and lift > 0
        for x in (21, 22):
            assert st.call("stairs.axes", [x, 20, 0]).eax == 0, x   # the deck tiles are never stairs
    finally:
        st.__exit__(None, None, None)


@need_exe
def test_one_tile_dry_dip_is_no_gap(portlib):
    """a 1-tile dip on unflattened land: lowering its corners lowers the banks' edges with them (shared vertices), so no gap"""
    st = _state(portlib, 50)
    try:
        _course(portlib, st)
        portlib.defect(SWITCH, False)
        for x in (34, 35, 36):
            _tile(st, x, 20, ROUGH, path=(x == 36))
        for vx in (35, 36):
            for vy in (19, 20):
                _vertex(st, vx, vy, LEVEL - 4)
        st.mem_write(st.grid_va("g_cornerCache"), bytes(8 * 50 * 50))
        assert st.call("crossings.edge", [34, 20, 0]).eax == LEVEL - 4
        assert _find(st, 34, 20) is None
    finally:
        st.__exit__(None, None, None)


# ---------------------------------------------------------------------------------------------------- the 0x2b thought on a deck
DX8 = (0, 1, 1, 1, 0, -1, -1, -1)   # f_tab_dirDx_4c2878 / f_tab_dirDy_4c2898 (as test_p39a_stairs)
DY8 = (-1, -1, 0, 1, 1, 1, 0, -1)
THOUGHT = 0x2B


def _deck_golfer_case(portlib, gap_on, g, ramp, deck):
    """test_p39a_stairs._golfer_case with layPath.stairs at 1 and a gap bridge entry over the golfer's walking line when `deck`: one
    updateGolfers call on mood5_fixture/00, a plus of path tiles around golfer g's tile, the corner levels rising 3 a tile along `ramp`;
    returns (thoughts +0x70..+0x79 before, after, the walking direction after)"""
    from port.test.test_defects_p28 import _seg_image
    from port.test.test_golfers import VL
    img = _seg_image("mood5_fixture", "00")
    portlib.defect(SWITCH, not gap_on)
    portlib.defect("layPath.stairs", True)   # the original stairs rule (_walker_on_flat leaves it at 0)
    o = {nm: v for nm, v, _ in portlib.objects()}
    n = 50
    with portlib.state() as st:
        st.set_data_base(VL.DATA_VA)
        st.write(0, img)
        r = o["g_golfers"] + g * 0x100
        x, y = struct.unpack("<ii", st.read(r, 8))
        tx, ty = x >> 10, y >> 10
        fl = bytearray(st.read(o["g_tileFlags"], 2 * n * n))
        for ax, ay in ((0, 0), (1, 0), (-1, 0), (0, 1), (0, -1), (2, 0), (-2, 0), (0, 2), (0, -2)):
            fl[2 * ((tx + ax) * n + ty + ay)] |= PATH
        st.write(o["g_tileFlags"], bytes(fl))
        axis, sign = ramp
        v = n + 1
        lv = bytearray(st.read(o["g_cornerLevel"], v * v))
        for lx in range(tx - 2, tx + 4):
            for ly in range(ty - 2, ty + 4):
                t = (lx - tx) if axis == 0 else (ly - ty)
                lv[lx * v + ly] = 9 + 3 * sign * max(-2, min(2, t))
        st.write(o["g_cornerLevel"], bytes(lv))
        st.write(o["g_cornerCache"], bytes(8 * n * n))
        st.call("crossings.clear", [])
        if deck:   # two entries covering tiles t - 2 .. t + 1 along the walk (the golfer's tile before and after its step)
            dx, dy = (1, 0) if axis == 0 else (0, 1)
            st.call("crossings.addGap", [tx - 2 * dx, ty - 2 * dy, axis, 2, 15])
            st.call("crossings.addGap", [tx, ty, axis, 2, 15])
        st.write(r + 0x14, struct.pack("<h", 0))   # a new step this call
        before = st.read(r + 0x70, 10)
        st.call("updateGolfers", [0])
        after = st.read(r + 0x70, 10)
        d = st.read(r + 0x1a, 1)[0] & 7
        st.call("crossings.clear", [])
        return before, after, d


@need_exe
def test_no_steep_slope_thought_on_a_deck(portlib):
    from port.test.test_p39a_stairs import _walker_on_flat
    g, d = _walker_on_flat(portlib)
    axis = 0 if DX8[d] != 0 else 1
    sign = DX8[d] if axis == 0 else DY8[d]
    b1, a1, _ = _deck_golfer_case(portlib, False, g, (axis, sign), True)    # switch at 1: the deck entry is not consulted
    b0, a0, d0 = _deck_golfer_case(portlib, True, g, (axis, sign), True)    # switch at 0 on a deck
    bn, an, _ = _deck_golfer_case(portlib, True, g, (axis, sign), False)    # switch at 0, no deck: the original's test
    assert a1[0] == THOUGHT and a1[1:] == b1[:9], (b1.hex(), a1.hex())
    assert an[0] == THOUGHT and an[1:] == bn[:9], (bn.hex(), an.hex())
    assert d0 == d and a0 == b0, (b0.hex(), a0.hex())
