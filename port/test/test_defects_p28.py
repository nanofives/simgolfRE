"""P28A (decision G30, D5 / D6): the grid-edge reads and writes as `play` switches.

Switch 1 (default) = the original's behaviour byte for byte, 0 = the fix. Each test puts two different fillers in the bytes a
site reads from outside the 50 x 50 grid and shows that at 1 the function's result follows the filler (the original's
dependence, so the test reaches the site) and at 0 it does not. Sites, addresses and the evidence for every fix:
log/roadmap/P28/P28A_notes.md. Run as one file per process: `pytest port/test/test_defects_p28.py`.

Not reached here (listed in the notes): mainLoop.edgeNeighbourType and mainLoop.edgeNeighbourNoFlags draw sprites only (a
six-frame segment run with the camera on the west edge changes no GameState byte besides the fillers); their default build is
checked by the segments gate and their effect by the play check's screenshots.
"""
from __future__ import annotations

import pathlib
import struct

import pytest

from port.test import lib as L
from port.test.test_terrain import new_state, objs
from port.test.test_terrain_gen import _fixed_image, _put, port_call

pytestmark = pytest.mark.port

SEGMENTS = pathlib.Path(__file__).resolve().parents[1] / "corpus" / "segments"
ATTR = 0x30          # TileTypeAttr record size
WALK = 0x22

SWITCHES = {
    "trialShot.edgeNeighbourType": "model_defects#5",
    "updateGolfers.edgeNeighbourType": "model_defects#5",
    "scanTileLine.edgeCellType": "model_defects#5",
    "mainLoop.edgeNeighbourType": "model_defects#5",
    "mainLoop.edgeNeighbourNoFlags": "model_defects#5",
    "floodMark.outsideNeighbour": "model_defects#6",
    "canStep.readBeforeBound": "model_defects#6",
    "heightBlend.rowPastEnd": "model_defects#6",
    "generateRandomCourse.riverOutside": "model_defects#6",
    "generateRandomCourse.smoothingOutside": "model_defects#6",
}


@pytest.fixture(autouse=True)
def _reset(portlib):
    portlib.defect_reset()
    yield
    portlib.defect_reset()


def test_switches_registered_default_on(portlib):
    d = portlib.defects()
    for name, ref in SWITCHES.items():
        assert name in d, name
        assert d[name]["ref"] == ref and d[name]["on"] == 1, name


# ------------------------------------------------------------------------------------------------ D6 floodMark
def _flood(lib, switch, filler):
    """floodMark(0, 5): the k = 6 neighbour is (-1, 5), tile_flags word -45 (before the grid). Returns that word after."""
    lib.defect("floodMark.outsideNeighbour", switch)
    o = objs(lib)
    with new_state(lib) as st:
        at, tt = o["g_tileTypeAttrs"][0], o["g_tileType"][0]
        st.write(at + 1 * ATTR + WALK, b"\x01")          # type 1 walkable: the centre recurses into its neighbours
        st.write(at + 2 * ATTR + WALK, b"\x00")          # type 2 not walkable: a marked (-1, 5) stops there
        st.write(tt, b"\x01" * 2500)
        st.write(tt - 45, b"\x02")                       # type byte of (-1, 5)
        fl = o["g_tileFlags"][0]
        st.write(fl - 90, struct.pack("<H", filler))     # flags word of (-1, 5)
        st.call("floodMark", [0, 5])
        centre = struct.unpack("<H", st.read(fl + 2 * 5, 2))[0]
        return struct.unpack("<H", st.read(fl - 90, 2))[0], centre


def test_flood_mark_outside_neighbour(portlib):
    # 1: a filler with 0x20 is recursed into and gets 0x40 ORed in (a write outside tile_flags); a filler without it is left
    assert _flood(portlib, 1, 0x20) == (0x60, 0x40)
    assert _flood(portlib, 1, 0x00) == (0x00, 0x40)
    # 0: the outside cell does not exist: neither filler is read or written
    assert _flood(portlib, 0, 0x20) == (0x20, 0x40)
    assert _flood(portlib, 0, 0x00) == (0x00, 0x40)


# ------------------------------------------------------------------------------------------------ D6 canStep
def _can_step(lib, switch, x, y, d, far=False):
    lib.defect("canStep.readBeforeBound", switch)
    o = objs(lib)
    with new_state(lib) as st:
        at, tt = o["g_tileTypeAttrs"][0], o["g_tileType"][0]
        st.write(at + 1 * ATTR + WALK, b"\x01")
        st.write(tt, b"\x01" * 2500)
        st.write(tt - 45, b"\x01")                       # (-1, 5): walkable type, flags 0: every test before tileBlocked passes
        if far:                                          # direction 6 points 0x1000000 rows away: no mapping covers its cell
            st.write(o["f_tab_dirDx_4c2878"][0] + 4 * d, struct.pack("<i", 0x1000000))
        try:
            return st.call("canStep", [x, y, d]).eax
        except L.SgError as e:
            return ("error", e.code)


def test_can_step_read_before_bound(portlib):
    # not distinguishable by result: every arm after the outside reads returns 0 (tileBlocked answers 1 outside)
    assert _can_step(portlib, 1, 0, 5, 6) == 0
    assert _can_step(portlib, 0, 0, 5, 6) == 0
    # in the grid both settings agree (a walkable, unflagged neighbour can be stepped on)
    assert _can_step(portlib, 1, 3, 5, 6) == _can_step(portlib, 0, 3, 5, 6) == 1
    # the read itself: with the neighbour's cell outside every mapping, 1 faults on the flags read (the original reads it),
    # 0 returns before it
    assert _can_step(portlib, 1, 0, 5, 6, far=True) == ("error", L.SG_E_FAULT)
    assert _can_step(portlib, 0, 0, 5, 6, far=True) == 0


# ------------------------------------------------------------------------------------------------ D6 heightBlend
def _hblend(lib, switch, filler):
    """heightBlend(49, 10) with g_date 0: the x + 1 type test reads tile_type[2510], one row past the grid."""
    lib.defect("heightBlend.rowPastEnd", switch)
    o = objs(lib)
    with new_state(lib) as st:
        tt = o["g_tileType"][0]
        st.write(o["g_heightRelief"][0], struct.pack("<i", 1))
        st.write(o["g_sampleGrid"][0], bytes([100]) * o["g_sampleGrid"][1])
        st.write(tt + 2510, bytes([filler]))
        return st.call("heightBlend", [49, 10]).eax


def test_height_blend_row_past_end(portlib):
    deep = _hblend(portlib, 1, 0)
    assert deep != 3, "the control must not answer 3 without the outside read"
    assert _hblend(portlib, 1, 0x11) == 3                 # 1: the byte past the grid read as water ends the blend
    assert _hblend(portlib, 0, 0x11) == deep               # 0: that cell is type 0x14, never 0x11
    assert _hblend(portlib, 0, 0) == deep


# ------------------------------------------------------------------------------------------------ D5 scanTileLine
def _scan(lib, switch, filler):
    """A straight scan on sandbox_basic/00's state (its direction tables are filled; a fresh state's are zero and the scan never
    leaves its first cell) from tile (2, 10) toward (-1, 10), on ground type 0: its last points are the cells of row -1 =
    tile_type[-50..-1] (the course table's tail), scored by walk byte and read again by the second line."""
    from port.test.test_golfers import VL
    img = _seg_image("sandbox_basic", "00")
    lib.defect("scanTileLine.edgeCellType", switch)
    o = {n: v for n, v, _ in lib.objects()}
    with lib.state() as st:
        st.set_data_base(VL.DATA_VA)
        st.write(0, img)
        tt = o["g_tileType"]
        st.write(tt, bytes(2500))
        st.write(tt - 50, bytes([filler]) * 50)
        return st.call("scanTileLine", [(2 << 10) + 0x200, (10 << 10) + 0x200, -1 & 0xFFFFFFFF, 10, 1000, 0, 0]).eax


def test_scan_tile_line_edge_cell_type(portlib):
    at1 = [_scan(portlib, 1, f) for f in (4, 9, 0x14)]
    assert len(set(at1)) == 3, f"1: the score follows the course table byte ({at1})"
    assert [_scan(portlib, 0, f) for f in (4, 9, 0x14)] == [at1[2]] * 3   # 0: the outside cells are type 0x14


# ------------------------------------------------------------------------------------------------ D5 trialShot
def _seg_image(scenario, index):
    p = SEGMENTS / scenario / index / "start.data.bin"
    if not p.exists():
        pytest.skip(f"no segment corpus: {p}")
    return p.read_bytes()


def _trial(lib, switch, filler):
    """golfer 0 of sandbox_basic/00 with its ball at x 0x60 (tile row 0) on ground type 0xc: its trial roll crosses the west
    edge, where the edge flag compares against the row before the grid (the course table). Returns the rest point."""
    from port.test.test_golfers import VL
    img = _seg_image("sandbox_basic", "00")
    lib.defect("trialShot.edgeNeighbourType", switch)
    o = {n: v for n, v, _ in lib.objects()}
    with lib.state() as st:
        st.set_data_base(VL.DATA_VA)
        st.write(0, img)
        rec = o["g_golfers"]
        st.write(rec + 0xd4, struct.pack("<ii", 0x60, 5 << 10))
        h = st.read(rec + 0x21, 1)[0]
        st.write(o["g_holes"] + h * 0x208 + 0x18, struct.pack("<ii", 0, 20))
        tt = o["g_tileType"]
        st.write(tt, bytes([0xC]) * 2500)
        st.write(tt - 50, bytes([filler]) * 50)
        st.call("trialShot", [0, 0, 0, 0])
        return struct.unpack("<ii", st.read(o["g_shot56a78c"], 8))


def test_trial_shot_edge_neighbour_type(portlib):
    same, other = _trial(portlib, 1, 0xC), _trial(portlib, 1, 0x14)
    assert same != other                                   # 1: the rest point follows the course table byte
    assert _trial(portlib, 0, 0xC) == _trial(portlib, 0, 0x14) == other   # 0: the outside neighbour is type 0x14


# ------------------------------------------------------------------------------------------------ D5 updateGolfers
def _roll(lib, switch, filler, ground, bx, by, speed, frames):
    """golfer 4 of mood5_fixture/00 (a rolling ball in the recording) moved to (bx, by), rolling toward -x with vz 0, on
    ground `ground`; the row before the grid holds `filler`. Returns the ball words after each frame."""
    from port.test.test_golfers import VL
    img = _seg_image("mood5_fixture", "00")
    lib.defect("updateGolfers.edgeNeighbourType", switch)
    o = {n: v for n, v, _ in lib.objects()}
    with lib.state() as st:
        st.set_data_base(VL.DATA_VA)
        st.write(0, img)
        rec = o["g_golfers"] + 4 * 0x100
        st.write(rec + 0xd4, struct.pack("<iiiiii", bx, by, 0, -0x40000000, speed, 0))
        tt = o["g_tileType"]
        st.write(tt, bytes([ground]) * 2500)
        st.write(tt - 50, bytes([filler]) * 50)
        out = []
        for _ in range(frames):
            st.call("updateGolfers", [0])
            out.append(st.read(rec + 0xd4, 0x18))
        return out


def test_update_golfers_edge_bits(portlib):
    """Ball at x 0x60: L3c <= 1 reads the (x - 1) neighbour (the edge bits, friction floor)."""
    args = (0xC, 0x60, 5 << 10, 0x400, 40)
    same, other = _roll(portlib, 1, 0xC, *args), _roll(portlib, 1, 0x14, *args)
    assert same != other
    assert _roll(portlib, 0, 0xC, *args) == _roll(portlib, 0, 0x14, *args) == other


def test_update_golfers_green_neighbour(portlib):
    """Ball at x 0x180 (L3c 6, no edge-bit read), fy 8 on a green: only the green test of the (x - 1) neighbour reads the
    course table byte (0xa there halves the speed)."""
    args = (0xA, 0x180, (5 << 10) + 0x200, 0x80, 12)
    same, other = _roll(portlib, 1, 0xA, *args), _roll(portlib, 1, 0x14, *args)
    assert same != other
    assert _roll(portlib, 0, 0xA, *args) == _roll(portlib, 0, 0x14, *args) == other


# ------------------------------------------------------------------------------------------------ D6 generateRandomCourse
def _gen(lib, river, smooth, rng=0x1234567, **kw):
    lib.defect("generateRandomCourse.riverOutside", river)
    lib.defect("generateRandomCourse.smoothingOutside", smooth)
    img = _fixed_image(lib, n=3, **kw)
    _put(lib, img, "g_rng", struct.pack("<I", rng & 0xFFFFFFFF))
    eax, out = port_call(lib, img, 3)
    assert out is not None, eax
    return out


def test_generate_river_column_50(portlib):
    """setting 0, course type 0, rng 0x1234567: the river steps along column 49, so column + 1 = 50 addresses tile
    (nrow + 1, 0) on the west edge; at 1 the shore type is written there and kept in g_tileTypeSaved (captured before the
    border sweep clears column 0 of g_tileType, and restored when that land is bought)."""
    o = objs(portlib)
    sv = o["g_tileTypeSaved"][0]
    one = _gen(portlib, 1, 1, loc=3, setting=0, ctype=0, difficulty=0, relief=0)
    zero = _gen(portlib, 0, 1, loc=3, setting=0, ctype=0, difficulty=0, relief=0)
    diff = [k for k in range(2500) if one[sv + k] != zero[sv + k]]
    assert diff, "the seed must reach the column-50 write"
    assert all(k % 50 == 0 for k in diff), "only west-edge tiles (column 0) differ"
    assert {one[sv + k] for k in diff} <= {0x8, 0xC, 0xA}, "1: a river shore type on the west edge"
    base = {zero[sv + k] for k in range(0, 2500, 50)}
    assert not ({zero[sv + k] for k in diff} & {0x8, 0xC}) or base <= {0x8, 0xC}
    # the rest of the image equals: the write is the only effect of the outside reads on this seed
    tt = o["g_tileType"][0]
    assert one[tt:tt + 2500] == zero[tt:tt + 2500]


def test_generate_smoothing_one_past(portlib):
    """Seed 2 of the U-1553 sweep (setting 1, course type 1, difficulty 2, relief 0): the smoothing of a row / column 50
    cell read as water (the next row's column 0, or past the grid) changes corner levels at 1; at 0 that cell is type 0x14
    and smooths nothing. Only g_cornerLevel differs."""
    o = objs(portlib)
    cl = o["g_cornerLevel"]
    kw = dict(rng=0x1234567 + 2 * 0x9E3779B1, loc=3, setting=1, ctype=1, difficulty=2, relief=0)
    one, zero = _gen(portlib, 1, 1, **kw), _gen(portlib, 1, 0, **kw)
    assert one[cl[0]:cl[0] + cl[1]] != zero[cl[0]:cl[0] + cl[1]]
    rest = [(a, b) for a, b in zip(one[:cl[0]] + one[cl[0] + cl[1]:], zero[:cl[0]] + zero[cl[0] + cl[1]:]) if a != b]
    assert not rest, "only corner levels differ"
