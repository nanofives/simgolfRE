"""Phase 3.1a terrain module: negative controls of every ported function, state tests of the functions without
vectors, and an OPT-IN differential check against the original (port/src/sim/terrain*.cpp, log/p3/p31a_notes.txt).

The opt-in check (`SG_EMU_ORACLE=1`, needs unicorn and the user's original/golf_clean.exe) runs the ORIGINAL function
in re/tools/vector_emu.py's Unicorn process over random GameState images and compares the whole data image and eax
with the port's. It is a development-time oracle for functions whose vectors are missing, thin or one-sided; the default
suite never runs original code (port/test/README.md).
"""
from __future__ import annotations

import ctypes as C
import os
import pathlib
import random
import struct
import sys
import zlib

import pytest

from port.test import lib as L
from port.test import vectors as V

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import vector_lib as VL  # noqa: E402

pytestmark = pytest.mark.port
need_corpus = pytest.mark.skipif(not VL.VEC_DIR.exists(), reason="vector corpus not present (port/corpus/vectors, local)")
need_exe = pytest.mark.skipif(not L.exe_path().exists(), reason="the user's golf_clean.exe is not present")
emu_oracle = pytest.mark.skipif(os.environ.get("SG_EMU_ORACLE") != "1",
                                reason="opt-in: SG_EMU_ORACLE=1 runs the original under Unicorn (development oracle)")

# (function, mode, what): wrong bodies the vectors must see (RED)
NEGCTL_RED = [
    ("freeAtTile", 1, "x >> 9"),
    ("pushTripleEntry", 1, "capacity 0xff"),
    ("setGrids", 1, "extra build-code write"),
    ("floodMark", 1, "neighbour test 0x20 only"),
    ("bilinearSample", 1, "floor division"),
    ("clearTile", 1, "setting 2 keeps the flags"),
    ("canStep", 1, "neighbour walk byte not tested"),
    ("evalPlacementArea", 1, "border cells ignored"),
    ("nearestPlaced", 1, "no early return"),
    ("findNearestTargetTile", 1, "distance bias 0x100"),
    ("sampleHeight", 1, "coarse weight 5"),
    ("rebuildTileCoverage", 1, "types >= 0x11 not marked"),
    ("relaxTile42f630", 1, "minimum not followed"),
    ("raiseFromNeighbours", 1, "sameByte ignored"),
    ("spreadShade", 1, "divisor + 1"),
    ("decorationName", 1, "category 0x12 takes 0x13's string"), ("decorationName", 2, "variant % 4"),
    ("buildToolLabel", 1, "0x8000 test skipped"), ("buildToolLabel", 2, "kind 0x10 takes kind 1's string"),
    ("heightBlend", 1, "lower clamp 2"), ("rateTile", 1, "toward_zero36 clamped to 2"),
    ("markTileKind11", 1, "variant 1"), ("spreadAllShade", 1, "total cap 0x6d5"), ("genNoise", 1, "range 0xf"),
    ("cornerRange", 1, "max never widens"), ("cornerHeights", 1, "corner 3 = corner 5"),
    ("tileQuery449310", 1, "cache skipped"), ("slopeY", 1, "half bound 0x1ff"), ("slopeX", 1, "left half reversed"),
    ("slopeMix", 1, "p / q swapped"), ("relaxEdges42f530", 1, "second corner pair untested"),
    ("heightAt42fa30", 1, "x weight 0x3ff"), ("rebuildHeightfield", 1, "flag 4 does not seed"),
]
# wrong bodies the vectors CANNOT see (recorded blind spots; the state tests or segments cover them)
NEGCTL_BLIND: list[tuple[str, int, str]] = [
    ("canStep", 2, "neighbour flag mask 0x100 only: no vector has a neighbour with flag 0x20 alone"),
    ("findNearestTargetTile", 2, "blocked cells counted: no blocked cell carries flag 0x200"),
    ("sampleHeight", 2, "no clamp at 0x200: no vector exceeds it"),
    ("rebuildTileCoverage", 2, "g_difficulty ignored: no difficulty-0 vector reaches a type 6..0x10 record"),
]
# functions without vectors: own state tests below (and the opt-in emulator check)
NO_VECTORS = ["raiseCornerLevelsAround", "lowerCornerLevelsAround", "propagateType11"]
TERRAIN = sorted({f for f, _, _ in NEGCTL_RED + NEGCTL_BLIND} | set(NO_VECTORS))


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
    """A blind spot that starts reading RED means the corpus grew: move it to NEGCTL_RED."""
    portlib.negctl(function, mode)
    assert vrunner.run_function(function).verdict == "GREEN", f"{function} mode {mode} ({what}) is no longer blind"


def test_every_terrain_function_has_a_negctl(portlib):
    funcs = portlib.functions()
    for f in TERRAIN:
        assert f in funcs, f
        assert portlib.negctl(f, 0) == 0, f


# ------------------------------------------------------------------------------------------- helpers
def objs(lib) -> dict[str, tuple[int, int]]:
    return {n: (o, s) for n, o, s in lib.objects()}


def new_state(lib):
    """A zero GameState at the original data base with the exe's const objects (direction tables, pools) and its other
    sections mapped read-only; needs the user's exe."""
    if not L.exe_path().exists():
        pytest.skip("the user's golf_clean.exe is not present")
    assert lib.exe_load() == ""
    st = lib.state()
    st.set_data_base(VL.DATA_VA)
    st.fill_from_exe()
    for name, va, data in V.exe_sections():
        if not (va < VL.DATA_VA + VL.DATA_SIZE and VL.DATA_VA < va + len(data)):
            st.map(va, data, readonly=True)
    return st


def corner(x, y):
    return x * 51 + y


# ------------------------------------------------------------------------------------------- no-vector functions
@pytest.mark.parametrize("mode", [0, 1])
def test_raise_corner_levels_around(portlib, mode):
    """Centre 10, kernel 2 everywhere but entry (1, 3) = -1: cells below 8 rise by one, the cell with kernel -1 rises
    while it is below 11; a cell exactly at 8 stays (strict <: mode 1 raises it); outside the 5 x 5 block nothing moves."""
    o = objs(portlib)
    lv, kn = o["g_cornerLevel"][0], o["g_levelFalloffKernel"][0]
    portlib.negctl("raiseCornerLevelsAround", mode)
    with new_state(portlib) as st:
        kern = [2] * 25
        kern[1 * 5 + 3] = -1
        st.write(kn, struct.pack("<25h", *kern))
        grid = bytearray(51 * 51)
        cx, cy = 20, 30
        grid[corner(cx, cy)] = 10
        cells = {(cx - 2, cy - 2): 3, (cx - 2, cy - 1): 8, (cx - 1, cy + 1): 10, (cx + 2, cy + 2): 7, (cx + 3, cy): 0,
                 (cx, cy - 3): 0, (cx + 1, cy): 255}
        for (x, y), v in cells.items():
            grid[corner(x, y)] = v
        st.write(lv, bytes(grid))
        st.call("raiseCornerLevelsAround", [cx, cy])
        g = st.read(lv, 51 * 51)
    want = dict(cells)
    want[(cx - 2, cy - 2)] = 4
    want[(cx - 1, cy + 1)] = 11          # kernel -1: limit 11
    want[(cx + 2, cy + 2)] = 8
    if mode == 1:
        want[(cx - 2, cy - 1)] = 9
    for (x, y), v in want.items():
        assert g[corner(x, y)] == v, ((x, y), g[corner(x, y)], v)
    # every other cell of the block was 0 < 8: raised to 1, the centre stays 10
    for dx in range(-2, 3):
        for dy in range(-2, 3):
            if (cx + dx, cy + dy) not in cells and (dx, dy) != (0, 0):
                assert g[corner(cx + dx, cy + dy)] == 1
    assert g[corner(cx, cy)] == 10


@pytest.mark.parametrize("mode", [0, 1])
def test_lower_corner_levels_around(portlib, mode):
    o = objs(portlib)
    lv, kn = o["g_cornerLevel"][0], o["g_levelFalloffKernel"][0]
    portlib.negctl("lowerCornerLevelsAround", mode)
    with new_state(portlib) as st:
        st.write(kn, struct.pack("<25h", *([3] * 25)))
        grid = bytearray([200] * (51 * 51))
        cx, cy = 5, 6
        grid[corner(cx, cy)] = 100
        cells = {(cx - 2, cy): 103, (cx, cy + 2): 104, (cx + 1, cy + 1): 50}
        for (x, y), v in cells.items():
            grid[corner(x, y)] = v
        st.write(lv, bytes(grid))
        st.call("lowerCornerLevelsAround", [cx, cy])
        g = st.read(lv, 51 * 51)
    assert g[corner(cx, cy)] == 100
    assert g[corner(cx, cy + 2)] == 103
    assert g[corner(cx + 1, cy + 1)] == 50
    assert g[corner(cx - 2, cy)] == (102 if mode == 1 else 103)
    assert g[corner(cx - 1, cy - 1)] == 199 and g[corner(cx + 3, cy)] == 200 and g[corner(cx - 2, cy - 3)] == 200


def _attrs(portlib, st, collar: dict[int, int]):
    off = objs(portlib)["g_tileTypeAttrs"][0]
    for t, c in collar.items():
        st.write(off + t * 0x30 + 0x26, bytes([c & 0xFF]))


@pytest.mark.parametrize("mode", [0, 1])
def test_propagate_type11(portlib, mode):
    """Variant bytes of a strip of collar-0x11 tiles: a cell with a non-0x11 unblocked neighbour gets 0, one with a
    0-variant neighbour (and no non-0x11 one) gets 1; a change recurses into the 0x11 neighbours only (mode 1: all)."""
    o = objs(portlib)
    ty, va = o["g_tileType"][0], o["g_tileVariant"][0]
    portlib.negctl("propagateType11", mode)
    with new_state(portlib) as st:
        _attrs(portlib, st, {1: 0x11, 2: 0x05, 20: 0})
        types = bytearray([20] * 2500)          # blocked everywhere ...
        var = bytearray([7] * 2500)
        for y in range(10, 15):                  # ... but a 3-wide strip of type 1 at x 9..11, y 10..14
            for x in (9, 10, 11):
                types[x * 50 + y] = 1
        types[10 * 50 + 16] = 2                  # a type-2 (collar 5) tile beyond the strip's end, not adjacent
        types[9 * 50 + 15] = 2                   # adjacent (diagonal) to (10, 14)
        var[10 * 50 + 9] = 0
        st.write(ty, bytes(types))
        st.write(va, bytes(var))
        st.call("propagateType11", [10, 14])
        got = st.read(va, 2500)
    # (10, 14): its neighbour (9, 15) is collar 5 -> 0; it changed (7 -> 0), so its 0x11 neighbours recompute:
    assert got[10 * 50 + 14] == 0
    # (9, 13), (11, 13), (10, 13), (9, 14), (11, 14): neighbours of (10,14) of collar 0x11; each sees (10,14)=0 -> 1,
    # except (9, 14) which also touches (9, 15) (collar 5) -> 0
    assert got[9 * 50 + 14] == 0
    for c in ((10, 13), (11, 13), (11, 14)):
        assert got[c[0] * 50 + c[1]] == 1, c
    if mode == 0:
        assert got[10 * 50 + 16] == 7 and got[9 * 50 + 15] == 7     # collar 5: never recursed into
    else:
        assert got[9 * 50 + 15] != 7


# ------------------------------------------------------------------------------------------- opt-in emulator oracle
def _rand_image(portlib, rng: random.Random) -> bytes:
    """The file's initialised .data plus random tile grids, a tile-type table from the exe's init image, placed objects."""
    o = objs(portlib)
    assert portlib.exe_load() == ""
    with portlib.state() as st:
        st.fill_from_exe(whole_image=True)
        img = bytearray(st.image())

    def put(name, data, at=0):
        off, size = o[name]
        assert at + len(data) <= size, name
        img[off + at:off + at + len(data)] = data

    init = o["f_tab_tileTypeAttrInit_4c1a40"]
    put("g_tileTypeAttrs", bytes(img[init[0]:init[0] + 23 * 0x30]))
    put("g_tileType", bytes(rng.choice([rng.randrange(23), 20, 0x11, 0x11, 1, 2]) for _ in range(2500)))
    put("g_tileFlags", b"".join(struct.pack("<H", rng.getrandbits(16) & rng.choice([0, 0x20, 0x420, 0x8040, 0xFFFF]))
                                for _ in range(2500)))
    put("g_tileVariant", bytes(rng.choice([0, 0, 1, rng.getrandbits(8)]) for _ in range(2500)))
    put("g_tileLevel", bytes(rng.randrange(256) for _ in range(2500)))
    put("g_cornerLevel", bytes(rng.randrange(256) for _ in range(2601)))
    put("g_levelFalloffKernel", struct.pack("<25h", *[rng.randrange(-4, 6) for _ in range(25)]))
    put("g_sampleGrid", bytes(rng.randrange(256) for _ in range(361)))
    put("g_tileShadeA", bytes(rng.randrange(256) for _ in range(2500)))
    put("g_tileShadeB", bytes(rng.randrange(256) for _ in range(2500)))
    recs = b""
    for i in range(256):
        t = rng.choice([-1, -1, rng.randrange(20)])
        recs += struct.pack("<hhhBBii", t, rng.randrange(2, 46), rng.randrange(2, 46), rng.randrange(4),
                            rng.getrandbits(8), rng.randrange(-2, 10), rng.randrange(100))
    put("g_placed_objects", recs)
    put("g_objectTypeLevel", struct.pack("<20i", *[rng.randrange(-2, 120) for _ in range(20)]))
    put("g_courseIndex", struct.pack("<i", rng.randrange(17)))
    put("g_courseType", bytes([rng.randrange(4)]))
    put("g_difficulty", struct.pack("<i", rng.randrange(3)))
    put("g_flags", struct.pack("<I", rng.getrandbits(32)))
    return bytes(img)


def emu_compare(portlib, name: str, addr: int, args: list[int], ret: str, image: bytes):
    """(port eax or None, emulated eax or None, list of differing GameState byte offsets); None = faulted."""
    import vector_emu as VE
    img = VE.load_image()
    res = VE.run_call(img, image, [], "default", ["int"] * len(args), args, ret, 0x027F, addr, budget=20_000_000)
    emu = bytearray(image)
    for a, b in res.mem.items():
        if VL.DATA_VA <= a < VL.DATA_VA + VL.DATA_SIZE:
            emu[a - VL.DATA_VA] = b
    emu_eax = None if res.err else res.eax
    with new_state(portlib) as st:
        st.load_image(image)
        try:
            port_eax = st.call(name, args).eax
            port = st.image()
        except L.SgError:
            port_eax, port = None, None
    if emu_eax is None or port_eax is None:
        return port_eax, emu_eax, []
    diff = [i for i in range(VL.DATA_SIZE) if port[i] != emu[i]] if port != bytes(emu) else []
    return port_eax, emu_eax, diff


def _emu_run(portlib, name, addr, ret, argsets, seed, mutate=None):
    """([(vector, args, port eax, emulated eax, first differing offsets)], runs where both faulted)"""
    rng = random.Random(seed)
    bad, faults = [], 0
    for k, args in enumerate(argsets):
        image = _rand_image(portlib, rng)
        if mutate:
            image = bytes(mutate(rng, bytearray(image)))
        pe, ee, diff = emu_compare(portlib, name, addr, args, ret, image)
        if pe is None and ee is None:
            faults += 1
        if (pe is None) != (ee is None) or diff or (ret != "void" and pe is not None and pe != ee):
            bad.append((k, args, pe, ee, diff[:8]))
    return bad, faults




def _set(portlib, img: bytearray, name: str, data: bytes, at: int = 0) -> bytearray:
    off, size = objs(portlib)[name]
    assert at + len(data) <= size, name
    img[off + at:off + at + len(data)] = data
    return img


def _m_golfers(portlib):
    """Golfer records with on-map world positions (balls, partners), a sparse mood / ball-stop grid, a path-cost grid."""
    def m(rng, img):
        recs = bytearray()
        for i in range(156):
            r = bytearray(0x100)
            struct.pack_into("<ii", r, 0, rng.randrange(0, 51200), rng.randrange(0, 51200))
            r[0x18] = rng.choice([0, 0, 0x20, 0x24])
            r[0x19] = rng.getrandbits(8)
            r[0x21] = rng.choice([1, 2, 0x13, 0xFF, 0])
            r[0x22] = rng.choice([0, 0, 3])
            r[0x36] = rng.randrange(-2, 6) & 0xFF
            struct.pack_into("<h", r, 0x16, rng.getrandbits(16) - 0x8000)
            r[0xBA] = rng.randrange(3)
            struct.pack_into("<ii", r, 0xD4, rng.randrange(0, 51200), rng.randrange(0, 51200))
            r[0xF0:0xFC] = bytes(rng.randrange(12) for _ in range(12))
            recs += r
        _set(portlib, img, "g_golfers", bytes(recs))
        _set(portlib, img, "g_tilePathCost", bytes(rng.choice([0, 1, 1, 2, 5, 0xFF]) for _ in range(2500)))
        _set(portlib, img, "g_ballStopCount", bytes(rng.choice([0] * 6 + [1, 3]) for _ in range(2500)))
        _set(portlib, img, "g_tileMoodUp", bytes(rng.choice([0] * 20 + [5, 40]) for _ in range(2500)))
        _set(portlib, img, "g_tileMoodDown", bytes(rng.choice([0] * 20 + [7, 90]) for _ in range(2500)))
        _set(portlib, img, "g_date", struct.pack("<i", rng.choice([0, 0, rng.getrandbits(20)])))
        _set(portlib, img, "g_activeObjectLevel", struct.pack("<20i", *[rng.randrange(0, 9) for _ in range(20)]))
        return img
    return m


def _m_flags_clear(portlib, mask: int, then=None):
    """Clear bits of g_flags (a debug overlay whose drawing the port drops by design)."""
    def m(rng, img):
        if then:
            img = then(rng, img)
        off = objs(portlib)["g_flags"][0]
        v = struct.unpack_from("<I", img, off)[0] & ~mask
        struct.pack_into("<I", img, off, v)
        return img
    return m


def _r(i, lo, hi, salt=0):
    return random.Random(i * 7919 + salt).randrange(lo, hi)


def oracle_cases(portlib):
    g = _m_golfers(portlib)
    return [
        # (function, address, ret, argument sets, image mutator)
        ("raiseCornerLevelsAround", 0x00406F20, "void", [[_r(i, 2, 49), _r(i, 2, 49, 1)] for i in range(40)], None),
        ("lowerCornerLevelsAround", 0x00406F90, "void", [[_r(i, 2, 49), _r(i, 2, 49, 1)] for i in range(40)], None),
        ("propagateType11", 0x0042F1C0, "void", [[_r(i, 0, 50), _r(i, 0, 50, 1)] for i in range(40)], None),
        ("freeAtTile", 0x00402930, "void", [[i % 50, (i * 7) % 50] for i in range(20)], None),
        ("evalPlacementArea", 0x0040DB90, "int",
         [[_r(i, 0, 48), _r(i, 0, 48, 1), i % 5, (0, 4, 0xC, 7, 0x10)[i % 5]] for i in range(60)], None),
        ("rebuildTileCoverage", 0x0042F340, "void", [[0] for _ in range(20)], None),
        ("spreadShade", 0x004615F0, "void", [[i % 50, (i * 11) % 50, (i * 37) % 600 - 300] for i in range(20)], None),
        ("canStep", 0x00407400, "int", [[_r(i, 0, 50), _r(i, 0, 50, 1), i % 8] for i in range(80)], None),
        ("findNearestTargetTile", 0x0040DE70, "void",
         [[_r(i, 0, 51200), _r(i, 0, 51200, 1), i % 6] for i in range(30)], None),
        ("nearestPlaced", 0x0040DDB0, "int", [[i % 20, _r(i, 0, 51200), _r(i, 0, 51200, 1)] for i in range(40)], None),
        ("sampleHeight", 0x0042DBA0, "int", [[_r(i, -4000, 9000), _r(i, -4000, 9000, 1)] for i in range(60)], None),
        ("heightBlend", 0x0040C170, "int", [[_r(i, -1, 51), _r(i, -1, 51, 1)] for i in range(80)], g),
        ("rateTile", 0x00422530, "int", [[_r(i, 0, 156)] for i in range(60)], g),
        ("markTileKind11", 0x0042F2C0, "void", [[] for _ in range(10)], None),
        ("spreadAllShade", 0x004616F0, "void", [[] for _ in range(10)], g),
        ("cornerHeights", 0x0040BFE0, "int",
         [[_r(i, -1, 51), _r(i, -1, 51, 1), _r(i, -1, 9, 2), i % 2] for i in range(80)], g),
        ("slopeX", 0x0040C3A0, "int", [[_r(i, 0, 51200), _r(i, 0, 51200, 1)] for i in range(60)], g),
        ("slopeY", 0x0040C2F0, "int", [[_r(i, 0, 51200), _r(i, 0, 51200, 1)] for i in range(60)], g),
        ("heightAt42fa30", 0x0042FA30, "int", [[_r(i, 0, 51200), _r(i, 0, 51200, 1)] for i in range(60)], g),
        ("rebuildHeightfield", 0x0042F7A0, "void", [[] for _ in range(6)], g),
        ("scanTileLine", 0x00421FA0, "int",
         [[_r(i, 0, 51200), _r(i, 0, 51200, 1), _r(i, 0, 50, 2), _r(i, 0, 50, 3), _r(i, 0, 300, 4), _r(i, -2, 3, 5),
           i % 2] for i in range(60)], g),
        ("pathStep", 0x0042E7E0, "int",
         [[_r(i, 0, 51200), _r(i, 0, 51200, 1), _r(i, 0, 51200, 2), _r(i, 0, 51200, 3), _r(i, 0, 152, 4)]
          for i in range(40)], _m_flags_clear(portlib, 0x1000, g)),
        ("buildToolLabel", 0x0040A160, "int", [[_r(i, -1, 51), _r(i, -1, 51, 1)] for i in range(60)], None),
        ("decorationName", 0x00407700, "int",
         [[_r(i, 0, 50), _r(i, 0, 50, 1), (-1, 0x100 | _r(i, 0, 256, 2), _r(i, 0, 23, 3))[i % 3]] for i in range(60)],
         None),
    ]


ORACLE_IDS = ["raiseCornerLevelsAround", "lowerCornerLevelsAround", "propagateType11", "freeAtTile", "evalPlacementArea",
              "rebuildTileCoverage", "spreadShade", "canStep", "findNearestTargetTile", "nearestPlaced", "sampleHeight",
              "heightBlend", "rateTile", "markTileKind11", "spreadAllShade", "cornerHeights", "slopeX", "slopeY",
              "heightAt42fa30", "rebuildHeightfield", "scanTileLine", "pathStep", "buildToolLabel", "decorationName"]


def _oracle_case(portlib, function):
    return next(c for c in oracle_cases(portlib) if c[0] == function)


@emu_oracle
@need_exe
@pytest.mark.parametrize("function", ORACLE_IDS)
def test_emulated_original_equals_port(portlib, function):
    _, addr, ret, argsets, mutate = _oracle_case(portlib, function)
    bad, faults = _emu_run(portlib, function, addr, ret, argsets, zlib.crc32(function.encode()), mutate)
    assert not bad, f"{function}: {len(bad)} of {len(argsets)} differ, first {bad[:3]}"
    assert faults <= len(argsets) // 2, f"{function}: {faults} of {len(argsets)} runs faulted in both (states too wild)"


@emu_oracle
@need_exe
@pytest.mark.parametrize("function,mode,what", [b for b in NEGCTL_BLIND if b[0] in ORACLE_IDS])
def test_emulator_oracle_sees_blind_spots(portlib, function, mode, what):
    """The wrong bodies the vectors cannot see read RED against the emulated original on random states."""
    _, addr, ret, argsets, mutate = _oracle_case(portlib, function)
    portlib.negctl(function, mode)
    bad, _ = _emu_run(portlib, function, addr, ret, argsets, zlib.crc32(function.encode()), mutate)
    assert bad, f"{function} mode {mode} ({what}) is blind to the oracle too"


# =========================================================================================== the Terrain.dll boundary
# The exe functions that call Terrain.dll (port/src/sim/terrain_dll.cpp) run against a recording fake Terrain
# (port/src/sim/terrain_fake.cpp). PyFakeTerrain below is its mirror: the same calls, eax values and memory effects,
# served to the ORIGINAL code in Unicorn at the Terrain.dll import traps and at the entries of the engine functions the
# port reaches through TerrainApi (input pump, window paint, blits, renderTerrainStrip, operator new / delete). A
# scenario compares the call logs, the GameState, the Terrain object and the heap block, and eax.
TERR_VA, HEAP_VA, LOG_VA, OBJ_VA = 0x30000000, 0x31000000, 0x32000000, 0x38000000
HEAP_SIZE, LOG_CAP = 0x40000, 0x80000
TILE_SIZE, TILES_AT = 0x248, 0x3A4
STUB_DC = OBJ_VA + 0x800             # the display surface's vtable slot +0x28 (initTerrain's acquire_dc)

# method ids (terrain_fake.cpp's enum)
(PASS_COLLAR, LOAD_COURSE, SET_TYPE, GET_VAR, LAY_PATH, SET_WALL, CALC_NORMALS, CALC_ALL, ELEVATE, LOWER, LOWER_EDGE,
 HAS_PATH, HAS_CONN, UPDATE_PATH, INIT_SYSTEM, INIT_TERRAIN, SET_ZOOM, RENDER, LOCAL_RENDER, PATH_RENDER, OS_MAJOR,
 ACQUIRE_DC, PUMP, PAINT, BLIT_FULL, BLIT_SCREEN, RENDER_STRIP, OP_NEW) = range(1, 29)
# Terrain.dll import (decorated name) -> (method id, stack arguments; thiscall: the callee pops them)
TER_IMPORTS = {
    "?passCollarInfo@Terrain@@QAEXQAHH@Z": (PASS_COLLAR, 2), "?loadNewCourseType@Terrain@@QAEXH@Z": (LOAD_COURSE, 1),
    "?setType@Terrain@@QAEXPAVTile@@HH@Z": (SET_TYPE, 3), "?getVariation@Terrain@@QAEHPAVTile@@@Z": (GET_VAR, 1),
    "?layPath@Terrain@@QAEXPAVTile@@HH@Z": (LAY_PATH, 3), "?setWall@Terrain@@QAEXPAVTile@@HH_N@Z": (SET_WALL, 4),
    "?calcNormals@Terrain@@QAEXPAVTile@@@Z": (CALC_NORMALS, 1), "?calcAllNormals@Terrain@@QAEXPAVTile@@@Z": (CALC_ALL, 1),
    "?elevateCorner@Terrain@@QAEXPAVTile@@H@Z": (ELEVATE, 2), "?lowerCorner@Terrain@@QAEXPAVTile@@H@Z": (LOWER, 2),
    "?lowerEdgeCorner@Terrain@@QAEXPAVTile@@H0M@Z": (LOWER_EDGE, 4), "?hasPath@Terrain@@QAE_NPAVTile@@@Z": (HAS_PATH, 1),
    "?hasConnectedPath@Terrain@@QAE_NHH@Z": (HAS_CONN, 2), "?updatePath@Terrain@@QAEXHHH@Z": (UPDATE_PATH, 3),
    "?initSystem@Terrain@@QAEXHHPAUHDC__@@_N@Z": (INIT_SYSTEM, 4), "?initTerrain@Terrain@@QAEXXZ": (INIT_TERRAIN, 0),
    "?setZoomLevel@Terrain@@QAEXH@Z": (SET_ZOOM, 1), "?render@Terrain@@QAE_NPAVTile@@M@Z": (RENDER, 2),
    "?localRender@Terrain@@QAEXPAVTile@@0M@Z": (LOCAL_RENDER, 3), "?pathUpdateRender@Terrain@@QAEXPAVTile@@M@Z": (PATH_RENDER, 2),
}
# exe functions served by the fake: entry -> (kind, stack arguments, callee pops)
EXE_STUBS = {0x00483BD0: ("pump", 0, False), 0x004808C0: ("paint", 1, True), 0x00475C60: ("blit", 7, True),
             0x0044A6E0: ("strip", 7, False), 0x004A57F1: ("new", 1, False), 0x004A4FFC: ("delete", 1, False),
             STUB_DC: ("dc", 0, True), 0x004481B0: ("sound", 5, False)}


def _rotl(v, n):
    return ((v << n) | (v >> (32 - n))) & 0xFFFFFFFF


def _mix(i, a):
    a = (list(a) + [0] * 6)[:6]
    return (i * 0x9E3779B1 ^ a[0] * 0x85EBCA6B ^ _rotl(a[1], 5) ^ a[2] * 0xC2B2AE35 ^ (a[3] << 11) ^ a[4] ^ (a[5] << 17)) & 0xFFFFFFFF


class PyFakeTerrain:
    """terrain_fake.cpp's FakeTerrain over Unicorn memory (rd / wr callbacks)."""

    def __init__(self, rd, wr, os_major):
        self.rd, self.wr, self.os_major, self.used, self.log = rd, wr, os_major, 0, []

    def _u32(self, va):
        return struct.unpack("<I", self.rd(va, 4))[0]

    def rec(self, i, a, value=None, whole=False):
        a = [x & 0xFFFFFFFF for x in a]
        eax = _mix(i, a)
        if value is not None:
            eax = value if whole else (eax & ~0xFF) | value
        self.log.append(tuple([i] + (a + [0] * 6)[:6] + [eax & 0xFFFFFFFF]))
        return eax & 0xFFFFFFFF

    def move(self, tile, c, d):
        if not tile:
            raise RuntimeError("fake Terrain: NULL tile")
        at = tile + 4 + 8 * (((c - 1) & 0xFFFFFFFF) >> 1 & 3)
        v = struct.unpack("<i", self.rd(at, 4))[0]
        self.wr(at, struct.pack("<I", (v + d) & 0xFFFFFFFF))

    def terrain(self, i, a):
        if i == PASS_COLLAR:
            p, n = a[0], a[1]
            h = 0x811C9DC5
            for b in self.rd(p, 4 * n):
                h = ((h ^ b) * 0x01000193) & 0xFFFFFFFF
            return self.rec(i, [h, n])
        if i == SET_TYPE:
            if a[0]:
                self.wr(a[0] + 0x24, struct.pack("<II", a[1], a[2]))
            return self.rec(i, a[:3])
        if i == GET_VAR:
            return self.rec(i, a[:1], self._u32(a[0] + 0x28) if a[0] else 0, True)
        if i == LAY_PATH:
            if a[0]:
                self.wr(a[0] + 0x2C, struct.pack("<I", (1 if a[1] else 0) | (2 if a[2] else 0)))
            return self.rec(i, a[:3])
        if i == SET_WALL:
            if a[0]:
                self.wr(a[0] + 0x234 + (a[1] & 7), bytes([a[2] & 0xFF if a[3] & 0xFF else 0]))
            return self.rec(i, [a[0], a[1], a[2], a[3] & 0xFF])
        if i == ELEVATE:
            self.move(a[0], a[1], +1)
            return self.rec(i, a[:2])
        if i in (LOWER, LOWER_EDGE):
            self.move(a[0], a[1], -1)
            return self.rec(i, a[:2] if i == LOWER else a[:4])
        if i == HAS_PATH:
            return self.rec(i, a[:1], (self._u32(a[0] + 0x2C) & 1) if a[0] else 0)
        if i == HAS_CONN:
            return self.rec(i, a[:2], (a[0] + a[1]) & 1)
        if i == INIT_SYSTEM:
            return self.rec(i, [a[0], a[1], a[2], a[3] & 0xFF])
        n = TER_NARGS[i]
        return self.rec(i, a[:n])

    def stub(self, kind, ecx, a):
        if kind == "pump":
            return self.rec(PUMP, [])
        if kind == "paint":
            return self.rec(PAINT, [])
        if kind == "blit":
            # Surface_blitThunk(this, src, ...): the full blit's this is the root window's surface (+0x274)
            if ecx == ROOT_SURFACE:
                return self.rec(BLIT_FULL, [])
            return self.rec(BLIT_SCREEN, [a[5], a[6]])
        if kind == "strip":
            return self.rec(RENDER_STRIP, a[1:7])
        if kind == "new":
            p = HEAP_VA + 0x100 + self.used
            self.used += (a[0] + 15) & ~15
            return self.rec(OP_NEW, [a[0]], p, True)
        if kind == "delete":
            return 0
        if kind == "dc":
            return self.rec(ACQUIRE_DC, [], 0xDC01, True)
        if kind == "sound":   # playSound: the audio backend (not logged: the port drops it without a registered one)
            return 0
        raise AssertionError(kind)


TER_NARGS = {i: n for i, n in TER_IMPORTS.values()}
ROOT_SURFACE = None   # set by _ter_setup from the GameState layout (g_rootWindow + 0x274)


def _ter_blocks(rng, w=50, h=50, heights=(0, 6)):
    """Terrain object (width / height, tiles with heights, type, variation, path bits and walls) and the heap block
    with an empty std::list head node at its start."""
    terr = bytearray(TILES_AT + 50 * 50 * TILE_SIZE)
    struct.pack_into("<ii", terr, 0x14, w, h)
    for k in range(50 * 50):
        t = TILES_AT + k * TILE_SIZE
        struct.pack_into("<iiii", terr, t + 4, *[rng.randrange(*heights) for _ in range(4)])
        struct.pack_into("<iii", terr, t + 0x24, rng.randrange(23), rng.choice([0, 0, 1, 2, 255]), rng.randrange(4))
        terr[t + 0x234:t + 0x23C] = bytes(rng.choice([0, 0, 0, 1, 3]) for _ in range(8))
    heap = bytearray(HEAP_SIZE)
    struct.pack_into("<II", heap, 0, HEAP_VA, HEAP_VA)          # the list's head node: next = prev = itself
    return terr, heap


def _ter_setup(portlib, rng, image: bytearray, w=50, h=50, heights=(0, 6)):
    """The GameState image and blocks of a Terrain scenario: gTerrain, the render list, the display surface object."""
    global ROOT_SURFACE
    o = objs(portlib)
    gt = o["gTerrain"][0]
    struct.pack_into("<I", image, gt, TERR_VA)
    tb = o["gTileBuffer"][0]
    struct.pack_into("<III", image, tb, 0, HEAP_VA, 0)
    root = o["g_rootWindow"][0]
    ROOT_SURFACE = VL.DATA_VA + root + 0x274
    struct.pack_into("<I", image, root + 0x278, OBJ_VA)
    obj = bytearray(0x1000)
    struct.pack_into("<I", obj, 0, OBJ_VA + 0x400)              # vtable
    struct.pack_into("<I", obj, 0x400 + 0x28, STUB_DC)          # slot +0x28
    terr, heap = _ter_blocks(rng, w, h, heights)
    return image, [(TERR_VA, terr), (HEAP_VA, heap), (OBJ_VA, obj)]


def emu_terrain_call(image: bytes, blocks, entry: int, args, os_major=6, budget=60_000_000):
    """Runs the original function at `entry` (cdecl, int arguments) over the image and blocks with PyFakeTerrain behind
    Terrain.dll and the engine stubs. -> (eax or None on a fault, final image, final blocks, fake log)"""
    import vector_emu as VE
    from unicorn import UC_ARCH_X86, UC_HOOK_CODE, UC_MODE_32, Uc, UcError
    from unicorn import x86_const as X
    img = VE.load_image()
    uc = Uc(UC_ARCH_X86, UC_MODE_32)
    uc.mem_map(VL.IMAGE_BASE, VE.IMAGE_END - VL.IMAGE_BASE)
    uc.mem_write(VL.IMAGE_BASE, img.data)
    uc.mem_write(VL.DATA_VA, bytes(image))
    for base, b in blocks:
        uc.mem_map(base & ~0xFFF, (len(b) + (base & 0xFFF) + 0xFFF) & ~0xFFF)
        uc.mem_write(base, bytes(b))
    traps = sorted(img.imports)
    tlo, thi = traps[0] & ~0xFFF, (traps[-1] + 0x1000) & ~0xFFF
    uc.mem_map(tlo, thi - tlo)
    uc.mem_write(tlo, b"\xCC" * (thi - tlo))
    uc.mem_map(VE.STACK_BASE, VE.STACK_SIZE)
    uc.mem_map(VE.SENTINEL, 0x1000)
    uc.mem_map(VE.TEB, 0x1000)
    uc.mem_map(VE.GDT, 0x1000)
    uc.mem_write(VE.TEB, struct.pack("<IIII", 0xFFFFFFFF, VE.STACK_BASE + VE.STACK_SIZE, VE.STACK_BASE, 0) + b"\0" * 8
                 + struct.pack("<I", VE.TEB))
    uc.mem_write(VE.GDT + 8, VE._gdt_entry(0, 0xFFFFF, 0xFB, 0xC))
    uc.mem_write(VE.GDT + 16, VE._gdt_entry(0, 0xFFFFF, 0xF3, 0xC))
    uc.mem_write(VE.GDT + 24, VE._gdt_entry(VE.TEB, 0xFFF, 0xF3, 0x4))
    uc.mem_write(VE.GDT + 32, VE._gdt_entry(0, 0xFFFFF, 0x93, 0xC))
    uc.reg_write(X.UC_X86_REG_GDTR, (0, VE.GDT, 0xFFF, 0))
    for reg, sel in ((X.UC_X86_REG_DS, 0x13), (X.UC_X86_REG_ES, 0x13), (X.UC_X86_REG_GS, 0x13),
                     (X.UC_X86_REG_FS, 0x1B), (X.UC_X86_REG_SS, 0x20)):
        uc.reg_write(reg, sel)
    uc.reg_write(X.UC_X86_REG_FPCW, 0x027F)
    sp = VE.STACK_BASE + VE.STACK_SIZE - 0x1000
    uc.mem_write(sp, struct.pack("<I", VE.SENTINEL) + b"".join(struct.pack("<I", a & 0xFFFFFFFF) for a in args))
    uc.reg_write(X.UC_X86_REG_ESP, sp)
    fake = PyFakeTerrain(lambda va, n: bytes(uc.mem_read(va, n)), lambda va, d: uc.mem_write(va, d), os_major)
    err = []

    def finish(eax, nargs, pops):
        esp = uc.reg_read(X.UC_X86_REG_ESP)
        ret = struct.unpack("<I", uc.mem_read(esp, 4))[0]
        uc.reg_write(X.UC_X86_REG_EAX, eax & 0xFFFFFFFF)
        uc.reg_write(X.UC_X86_REG_ESP, esp + 4 + (4 * nargs if pops else 0))
        uc.reg_write(X.UC_X86_REG_EIP, ret)

    def stack_args(n):
        esp = uc.reg_read(X.UC_X86_REG_ESP)
        return list(struct.unpack(f"<{n}I", uc.mem_read(esp + 4, 4 * n))) if n else []

    def on_code(u, addr, size, _):
        try:
            if addr in img.imports:
                name = img.imports[addr].split("!", 1)[1]
                if name == "GetVersionExA":
                    info = stack_args(1)[0]
                    u.mem_write(info + 4, struct.pack("<I", os_major))
                    fake.rec(OS_MAJOR, [], os_major, True)
                    return finish(1, 1, True)
                if name not in TER_IMPORTS:
                    raise RuntimeError(f"import {img.imports[addr]}")
                i, n = TER_IMPORTS[name]
                return finish(fake.terrain(i, stack_args(n)), n, True)
            kind, n, pops = EXE_STUBS[addr]
            return finish(fake.stub(kind, uc.reg_read(X.UC_X86_REG_ECX), stack_args(n)), n, pops)
        except Exception as e:  # noqa: BLE001 - any fake failure stops the run as a fault
            err.append(str(e))
            u.emu_stop()

    uc.hook_add(UC_HOOK_CODE, on_code, None, tlo, thi - 1)
    for a in EXE_STUBS:
        uc.hook_add(UC_HOOK_CODE, on_code, None, a, a)
    try:
        uc.emu_start(entry, VE.SENTINEL, count=budget)
    except UcError as e:
        err.append(str(e))
    if not err and uc.reg_read(X.UC_X86_REG_EIP) != VE.SENTINEL:
        err.append("instruction budget")
    final = bytes(uc.mem_read(VL.DATA_VA, VL.DATA_SIZE))
    out_blocks = [bytes(uc.mem_read(base, len(b))) for base, b in blocks]
    return (None if err else uc.reg_read(X.UC_X86_REG_EAX)), final, out_blocks, fake.log, err


def port_terrain_call(portlib, name, image: bytes, blocks, args, os_major=6):
    """The port's function with terrain_fake behind TerrainApi. -> (eax or None, final image, final blocks, log)"""
    with new_state(portlib) as st:
        st.load_image(image)
        bufs = []
        for base, b in blocks:
            buf = (C.c_uint8 * len(b)).from_buffer_copy(bytes(b))
            st.map(base, buf)
            bufs.append(buf)
        log = (C.c_uint8 * (4 + LOG_CAP * 32))()
        st.map(LOG_VA, log)
        st.call("terrainFake::install", [LOG_VA, LOG_CAP, HEAP_VA + 0x100, HEAP_SIZE - 0x100, os_major])
        try:
            eax = st.call(name, args).eax
        except L.SgError:
            eax = None
        st.call("terrainFake::remove", [])
        final = st.image()
        raw = bytes(log)
        n = struct.unpack_from("<I", raw, 0)[0]
        recs = [struct.unpack_from("<8I", raw, 4 + 32 * k) for k in range(min(n, LOG_CAP))]
        assert n <= LOG_CAP, f"{name}: {n} fake Terrain calls overflow the log ({LOG_CAP})"
        return eax, final, [bytes(b) for b in bufs], recs


def terrain_compare(portlib, name, entry, image, blocks, args, os_major=6, void=False):
    """'' when the port equals the emulated original (eax, log, GameState, blocks; both faulting counts as equal),
    else the first difference."""
    pe, pimg, pblk, plog = port_terrain_call(portlib, name, image, blocks, args, os_major)
    ee, eimg, eblk, elog, err = emu_terrain_call(image, blocks, entry, args, os_major)
    if pe is None or ee is None:
        return "" if pe is None and ee is None else f"fault: port {pe} emulated {ee} {err[:1]}"
    if pe != ee and not void:
        return f"eax port {pe:#x} emulated {ee:#x}"
    for k, (a, b) in enumerate(zip(plog, elog)):
        if tuple(a) != tuple(b):
            return f"log record {k}: port {[hex(v) for v in a]} emulated {[hex(v) for v in b]}"
    if len(plog) != len(elog):
        return f"log length port {len(plog)} emulated {len(elog)}"
    if pimg != eimg:
        i = next(i for i in range(len(pimg)) if pimg[i] != eimg[i])
        return f"GameState byte {i:#x} port {pimg[i]:#x} emulated {eimg[i]:#x}"
    for (base, _), a, b in zip(blocks, pblk, eblk):
        if a != b:
            i = next(i for i in range(len(a)) if a[i] != b[i])
            return f"block {base:#x} + {i:#x} port {a[i]:#x} emulated {b[i]:#x}"
    return ""


def _ter_flags(portlib, rng, img, bits, density=0.03):
    """Sparse tile flags: dense path flags make every togglePath re-path the whole grid (millions of fake calls)."""
    off = objs(portlib)["g_tileFlags"][0]
    for k in range(2500):
        struct.pack_into("<H", img, off + 2 * k, (rng.getrandbits(16) & bits) if rng.random() < density else 0)
    return img


def terrain_cases(portlib):
    """function -> (entry, [(args, image mutator(rng, image) -> image, terrain width, height)])"""
    o = objs(portlib)

    def m_tiles(rng, img):  # sparse path / object flags; corner levels of a real course (3..8, not 0..255)
        off, size = o["g_cornerLevel"]
        img[off:off + size] = bytes(rng.randrange(3, 9) for _ in range(size))
        return _ter_flags(portlib, rng, img, rng.choice([0x20, 0x60, 0x460, 0x1020]))

    def m_render(rng, img):  # updateTerrainRender's camera memory and flags
        img = m_tiles(rng, img)
        for name, v in (("g_terrainInitPending", rng.choice([0, 0, 1])), ("g_terrainLastZoom", rng.choice([1, 2, 2]))):
            off, size = o[name]
            img[off:off + size] = v.to_bytes(size, "little")
        for name, v in (("g_terrainLastCamX", rng.randrange(20, 30)), ("g_terrainLastCamY", rng.randrange(20, 30)),
                        ("g_zoomScale", rng.choice([1, 2, 4])), ("g_screenWidth", 800), ("g_screenHeight", 600)):
            struct.pack_into("<i", img, o[name][0], v)
        img[o["g_terrain820f2a"][0]:o["g_terrain820f2a"][0] + 3] = bytes(rng.choice([0, 0, 1]) for _ in range(3))
        struct.pack_into("<iii", img, o["g_terrain820f30"][0], 800, 600, rng.choice([0, 2, 2, 4]))
        wm = o["g_tileWallMask"][0]
        img[wm] = rng.choice([0, 2, 8])
        return img

    def none(rng, img):
        return img

    c = {}
    def m_ctype(v):
        def m(rng, img):
            img[o["g_courseType"][0]] = v
            return img
        return m

    c["applyCourseType"] = (0x00449400, [([i % 5], m_ctype(i % 2)) for i in range(8)])
    c["togglePath"] = (0x0044A410, [([TERR_VA + TILES_AT + (i * 51 % 2500) * TILE_SIZE, i * 51 // 50 % 50, (i * 7 + i // 2) % 50],
                                     m_tiles) for i in range(40)])
    c["rebuildTerrainGrid"] = (0x00449540, [([], m_tiles) for _ in range(4)])
    c["applyTileElevation"] = (0x00449FA0, [([TERR_VA + TILES_AT + (i * 37 % 2500) * TILE_SIZE, i * 37 // 50 % 50,
                                              i * 37 % 50, (i * 7) % 50, (i * 11) % 50], m_tiles) for i in range(40)])
    c["initTerrain"] = (0x00449790, [([800, 600], m_tiles) for _ in range(3)] + [([640, 480], m_ctype(1))])
    c["rebuildTileData"] = (0x0044A5B0, [([(i * 13) % 50, (i * 7) % 50], m_render) for i in range(6)])
    def m_unmoved(x, y, zoom, rot):  # the camera did not move: the tile-data rebuild and local-render branch
        def m(rng, img):
            img = m_render(rng, img)
            for name, v in (("g_terrainLastCamX", x), ("g_terrainLastCamY", y), ("g_terrainLastZoom", zoom)):
                struct.pack_into("<i", img, o[name][0], v)
            img[o["g_terrainInitPending"][0]] = 0
            img[o["g_terrain820f2a"][0]] = 0
            struct.pack_into("<i", img, o["g_terrain820f30"][0] + 8, rot)
            img[o["g_tileWallMask"][0]] = rng.choice([0, 0, 8])
            return img
        return m

    c["updateTerrainRender"] = (0x004498A0, [([20 + i, 25 - i, (1, 2)[i % 2], (0, 2)[i % 2]],
                                               m_unmoved(20 + i, 25 - i, (1, 2)[i % 2], (0, 2)[i % 2])) for i in range(4)]
                                  + [([20 + i % 7, 20 + (i * 3) % 7, (1, 2, 4)[i % 3], (0, 2, 4, 6, 1)[i % 5]],
                                      m_render) for i in range(24)])
    c["applyTileVariation"] = (0x00449F00, [([TERR_VA + TILES_AT + (i * 29 % 2500) * TILE_SIZE, i * 29 // 50 % 50,
                                              i * 29 % 50], m_tiles) for i in range(40)])
    c["applyTileWalls"] = (0x0044A380, [([TERR_VA + TILES_AT + (i * 23 % 2500) * TILE_SIZE, i * 23 // 50 % 50,
                                          i * 23 % 50], m_tiles) for i in range(40)])
    return c


TERRAIN_VOID = {"rebuildTileData"}    # eax not part of the interface (the only caller overwrites it)
TERRAIN_DLL = ["applyCourseType", "togglePath", "rebuildTerrainGrid", "applyTileElevation", "initTerrain",
               "rebuildTileData", "updateTerrainRender", "applyTileVariation", "applyTileWalls"]
TERRAIN_DLL_NEGCTL = [("applyCourseType", 1, "collar group 3"), ("togglePath", 1, "connected sweep skipped"),
                      ("rebuildTerrainGrid", 1, "corners one step short"), ("applyTileElevation", 1, "no edge test"),
                      ("initTerrain", 1, "version flag never set"), ("rebuildTileData", 1, "push on elevation only"),
                      ("updateTerrainRender", 1, "no tile-data rebuild"), ("applyTileVariation", 1, "type 6 no normals"),
                      ("applyTileWalls", 1, "removed wall keeps height 1")]


def _terrain_run(portlib, function, limit=None, first=False):
    entry, cases = terrain_cases(portlib)[function]
    rng = random.Random(zlib.crc32(b"ter" + function.encode()))
    bad = []
    for k, (args, mutate) in enumerate(cases[:limit]):
        image = bytearray(_rand_image(portlib, rng))
        image = mutate(rng, image)
        image, blocks = _ter_setup(portlib, rng, image)
        why = terrain_compare(portlib, function, entry, bytes(image), blocks, args, void=function in TERRAIN_VOID)
        if why:
            bad.append((k, args, why))
            if first:
                break
    return bad, len(cases[:limit])


@emu_oracle
@need_exe
@pytest.mark.parametrize("function", TERRAIN_DLL)
def test_terrain_boundary_equals_emulated_original(portlib, function):
    bad, n = _terrain_run(portlib, function)
    assert not bad, f"{function}: {len(bad)} of {n} differ, first {bad[:2]}"


@emu_oracle
@need_exe
@pytest.mark.parametrize("function,mode,what", TERRAIN_DLL_NEGCTL)
def test_terrain_boundary_negctl_reads_red(portlib, function, mode, what):
    portlib.negctl(function, mode)
    bad, n = _terrain_run(portlib, function, first=True)
    assert bad, f"{function} mode {mode} ({what}) reads GREEN against the emulated original"


# =========================================================================================== golden digests (default runs)
# The default suite never runs original code. For the functions the vectors cover thinly or not at all, the opt-in
# oracle above was run once and the emulated ORIGINAL's results of a fixed set of scenarios are pinned here as SHA-256
# digests (eax, the fake Terrain log, the final GameState and blocks: no game bytes). Every default run recomputes them
# from the PORT and compares. Regenerate (after checking the oracle is GREEN): SG_EMU_ORACLE=update pytest -k golden -s,
# then paste the printed dict.
import hashlib  # noqa: E402

GOLDEN_CASES = 6   # scenarios per function (the first ones of each oracle table)
GOLDEN_N = {"applyTileVariation": 40}   # more where 6 do not reach every negative control


def _digest(eax, img, blocks=(), log=()):
    h = hashlib.sha256()
    h.update(b"none" if eax is None else struct.pack("<I", eax & 0xFFFFFFFF))
    h.update(bytes(img))
    for b in blocks:
        h.update(bytes(b))
    for r in log:
        h.update(struct.pack("<8I", *r))
    return h.hexdigest()[:24]


def _golden_port(portlib, function):
    """Digests of the first GOLDEN_CASES scenarios, computed from the port."""
    out = []
    if function in TERRAIN_DLL:
        entry, cases = terrain_cases(portlib)[function]
        rng = random.Random(zlib.crc32(b"ter" + function.encode()))
        for args, mutate in cases[:GOLDEN_N.get(function, GOLDEN_CASES)]:
            image = mutate(rng, bytearray(_rand_image(portlib, rng)))
            image, blocks = _ter_setup(portlib, rng, image)
            pe, pimg, pblk, plog = port_terrain_call(portlib, function, bytes(image), blocks, args)
            if pe is None:
                out.append(_digest(None, b''))
            else:
                out.append(_digest(None if function in TERRAIN_VOID else pe, pimg, pblk, plog))
        return out
    _, addr, ret, argsets, mutate = _oracle_case(portlib, function)
    rng = random.Random(zlib.crc32(function.encode()))
    for args in argsets[:GOLDEN_N.get(function, GOLDEN_CASES)]:
        image = _rand_image(portlib, rng)
        if mutate:
            image = bytes(mutate(rng, bytearray(image)))
        with new_state(portlib) as st:
            st.load_image(image)
            try:
                eax = st.call(function, args).eax
                img = st.image()
            except L.SgError:
                eax, img = None, b""
        out.append(_digest(None if ret == "void" else eax, img))
    return out


def _golden_emulated(portlib, function):
    """The same digests from the emulated original (opt-in regeneration)."""
    out = []
    if function in TERRAIN_DLL:
        entry, cases = terrain_cases(portlib)[function]
        rng = random.Random(zlib.crc32(b"ter" + function.encode()))
        for args, mutate in cases[:GOLDEN_N.get(function, GOLDEN_CASES)]:
            image = mutate(rng, bytearray(_rand_image(portlib, rng)))
            image, blocks = _ter_setup(portlib, rng, image)
            ee, eimg, eblk, elog, err = emu_terrain_call(bytes(image), blocks, entry, args)
            if ee is None:
                out.append(_digest(None, b''))
            else:
                out.append(_digest(None if function in TERRAIN_VOID else ee, eimg, eblk, elog))
        return out
    import vector_emu as VE
    _, addr, ret, argsets, mutate = _oracle_case(portlib, function)
    rng = random.Random(zlib.crc32(function.encode()))
    for args in argsets[:GOLDEN_N.get(function, GOLDEN_CASES)]:
        image = _rand_image(portlib, rng)
        if mutate:
            image = bytes(mutate(rng, bytearray(image)))
        res = VE.run_call(VE.load_image(), image, [], "default", ["int"] * len(args), args, ret, 0x027F, addr,
                          budget=20_000_000)
        emu = bytearray(image)
        for a, b in res.mem.items():
            if VL.DATA_VA <= a < VL.DATA_VA + VL.DATA_SIZE:
                emu[a - VL.DATA_VA] = b
        out.append(_digest(None if (ret == "void" or res.err) else res.eax, b"" if res.err else bytes(emu)))
    return out


GOLDEN_FUNCTIONS = ["raiseCornerLevelsAround", "lowerCornerLevelsAround", "propagateType11", "markTileKind11",
                    "spreadAllShade", "rebuildHeightfield", "cornerHeights", "heightAt42fa30", "pathStep"] + TERRAIN_DLL
GOLDEN: dict[str, list[str]] = {
    'raiseCornerLevelsAround': ['4599f8061c967a753d556ae7', 'f7213861c81a97f5ded53150', 'fc877cc0106d8e3170e4f9c4', '40bbdacd4adbbcf09b6fd3e3', 'e9e703611700bd11c6b137b1', '9cd07ec1e2549115c852437a'],
    'lowerCornerLevelsAround': ['cf45d8f7b3ea20b605ad356a', '54d1a03fecbabecbe7871507', '65b245a9c653fedc9dcf12bd', '8c948ed602ea46659920c5a7', 'd9d2b13c744b6363f81f90b9', '64e6f0c5da94416e62e40013'],
    'propagateType11': ['3593cc06dcfd217087551612', '6e62e66840e64b16f7075be4', '57ab4a8c151989cd74258ea6', '67ecf544d9bffb51c3b18646', 'ff30486eafeeead8f0a7164f', 'd84c53fe4653a0d3e9db16dc'],
    'markTileKind11': ['ecae551fef7f99d75baf52ab', '31d0ca0306f27f0eced2aaa6', 'fa27bbc0b366e38bf2716f43', '81324c10cb09f17bdf3886da', 'e23eb491405b7006add3a74a', '80f2be3f3058b86947e63152'],
    'spreadAllShade': ['66368e2527d79174a3a0ab74', 'dc235706285d707dee8fedb1', '7dee4604a14e33f1a79732f2', '5b94fda0349b06901a151213', '254436862304b870c41f058b', '7463dee29d55c03d645c70ad'],
    'rebuildHeightfield': ['fd74e78f8dcaf6bc73ed61a4', 'e6231193c679abbe1f20761e', 'dc03a943bd1b88c4c6f4e86b', 'c8af1122af4e225236bad3c6', 'd70645c9d3510ee3dc8cfd1e', '368ad528b03c4fe7950b6758'],
    'cornerHeights': ['d3db49a5e1961b0be132ca2d', 'cc94bffd5952c6ce4636aad2', '126020aefea1bb56de52af18', 'd7ef9c85fb8b0613301e722d', '7c0f640a3227243c9552e403', 'df59266e09dcf694f242cd47'],
    'heightAt42fa30': ['7efd2da42c71c8af206e88f1', 'cdceaeae3b96246fc691bc16', '49f08b1c851af3199971c63a', '5fde40e7ef0b992c6e8aa7de', '1328517ab1a05aee6c317338', '327e670e5194d3cc3b5f9d6e'],
    'pathStep': ['6c94911f2dd8881176826102', '02affe0df922776a86107cd4', 'ff15370118cee45df493267a', 'a80a72676dc3a7da8014c1ab', 'f2c787b6448edb3eb54e69ca', 'a6add8c9fe38feb585945697'],
    'applyCourseType': ['d7bf1d90a45361977851c14a', 'bd1535a49ff9df9fadfc3496', '24616be86e774f2f31e3e2ff', '4f9e83725f385359764d74bf', '1515d3e5753cbb6f41f25373', '9945ee2f27630e9e3e4e6685'],
    'togglePath': ['929a1d42c41235a86ec42ba3', '274cda201e9fafa797aaea41', '6bbb64a7a2d80fdbe4f3aad1', '668dfca14f483f4df15c4b5c', 'd3641697ce79b964f7ef3785', '239672bc89c773c0cdf8ccd3'],
    'rebuildTerrainGrid': ['627c44688fd7ef2b620b5625', '70f89d732a4f8857adf2ca47', '2f1471b689e1d53af3ea1227', '7608fe1d5e4d0e6e188f46c8'],
    'applyTileElevation': ['c4dda57808a2d686c7282bf1', '2afab3dd0bf0ef40ce915f54', '5d427a0f20c1623ab344f2cc', '35d6f53cd915373846383b65', 'e8f286328d359487fc2d0f74', '1da4ea4a748235c58e65cf9c'],
    'initTerrain': ['f47182281c83586bddc8df83', '2126e69945484395b92cb91a', '9f5b059535ff9bcbc94884b7', '86b40c55171647fc47073734'],
    'rebuildTileData': ['8573edfaf893d283f0760b92', 'a4c14d77b9cd5bf7f40c93ee', 'e6c522ea2cc0a28993f0e6d6', 'a38d3a2266b615ef7526f781', 'cc3a4c98b8660fd54f63238f', '12e2bc0bcaa1511abfa79554'],
    'updateTerrainRender': ['a824a37d27e2fd9e8227c09a', '05eaa152b45bd999477af810', 'fe6d6f10d8ea024f9ba708a3', '071910ae0f47dc98fe583671', 'c9cfc8a21188f851f43fc18b', 'b3dd69d70c4b5d0a3612118e'],
    'applyTileVariation': ['00b4690ff932a9e811191a2b', '6aad555f7ce9ede33c81a18f', '19f83aa36f83ab9fb86ed69a', '19c6826e0e4be99d579f94a3', 'a194479b998dcc04f54356fd', '656917ce47e8ed025bbcfabf', '4f5725a5d7e9eac41f1e6fad', '7d31f66f3a05a65a61079245', '6b7b781b27f11a6c3927fd6f', '9326c389f803626bb0484ef4', 'a0869201736a9bfc25c3cdf7', 'c312861315972751c187b16f', '43f10e94eddc6ab35f9d2976', 'bf3f3a4a7284666d347f6827', '2a82d3821b59d4a5903d32db', 'fa967041ed62f3e402fa5875', 'c699f5d35c089efab96f0dce', '2c2595c3ca68aebfc44c8e1c', '41794fabf447acb83656aa88', 'e60f7f6a7e8abf9265305e5c', 'ee17b84645350c07ae5423cc', 'b0c248e0cddbcc2f33dfba4c', '6e384833768b12f6f9f8ac80', '645f719ddac6b226fea36f58', '38f6b00ea84fc6e557c02d40', '148df5b9ca9d866c4302d7e8', 'feb5f9b02108d84373ab4ead', '8b53f839f688091ef7ea3a48', '446575748017cadf09f764af', '9e3943306925f3c418eb3661', '8663bd1c8dd61f5cb91cedb9', '25df1de17eb626f3333aec3b', 'c18ca68da1d9b8428d33f4cf', '0d20811ca4a39465c938e5d6', '3081e3bf6ec91d51e4b94a4e', 'd0472cf8f399ba69391ddb8e', '4f2a4fa6f568336eac0bf720', '048072155e6704e6718d8866', '3c4b7b747c616e6ed7799d6f', 'c598af568522da377dbdceae'],
    'applyTileWalls': ['0423425aaa037ef8eaf93941', 'fe929cdcc604fc98a447452c', 'f2e7873d70321c10b0044a4d', 'f53945e4f0e9870c4d0cd728', 'e00260dce6a969cf4c0eb384', '2103def5e6debc2e95f9cab4'],
}


@need_exe
@pytest.mark.parametrize("function", GOLDEN_FUNCTIONS)
def test_golden_digests(portlib, function):
    """The port reproduces the pinned results of the emulated original (no original code runs here)."""
    if os.environ.get("SG_EMU_ORACLE") == "update":
        emu = _golden_emulated(portlib, function)
        print(f"GOLDEN[{function!r}] = {emu!r}")
        assert emu == _golden_port(portlib, function), f"{function}: the port differs from the emulated original"
        return
    if function not in GOLDEN:
        pytest.skip(f"{function}: no pinned digests (regenerate with SG_EMU_ORACLE=update)")
    assert _golden_port(portlib, function) == GOLDEN[function]


# =========================================================================================== customizeCharacter (no vectors)
def _m_customize(portlib, with_skills=True):
    g = _m_golfers(portlib)

    def m(rng, img):
        img = g(rng, img)
        o = objs(portlib)
        if with_skills:
            off, size = o["g_playerSkills"]
            img[off:off + size] = bytes(rng.choice([0, 1, 3]) for _ in range(size))
            img[off] = img[off] or 1                       # a skill is set: the character intro is not shown
        struct.pack_into("<i", img, o["g_nextGolferSlot"][0], rng.randrange(0, 0x98))
        struct.pack_into("<i", img, o["g_difficulty"][0], rng.randrange(4))
        struct.pack_into("<ii", img, o["g_walkers"][0], rng.randrange(51200), rng.randrange(51200))
        img[o["g_walkers"][0] + 0x16] = rng.randrange(8)
        # golfer records free for updateMembership (hole byte 0) with partner -1
        return img
    return m


def _customize_compare(portlib, image, with_pro):
    ee, eimg, _, _, err = emu_terrain_call(image, [], 0x0040F190, [with_pro])
    with new_state(portlib) as st:
        st.load_image(image)
        try:
            pe = st.call("customizeCharacter", [with_pro]).eax
        except L.SgError as e:
            return f"port error {e}" if ee is not None else ""
        pimg = st.image()
    if ee is None:
        return f"emulated fault {err[:1]}"
    if pe != ee:
        return f"eax port {pe:#x} emulated {ee:#x}"
    if pimg != eimg:
        i = next(i for i in range(len(pimg)) if pimg[i] != eimg[i])
        return f"GameState byte {i:#x} port {pimg[i]:#x} emulated {eimg[i]:#x}"
    return ""


@emu_oracle
@need_exe
@pytest.mark.parametrize("mode", [0, 1])
def test_customize_character_equals_emulated_original(portlib, mode):
    """withPro = 0 (the pro path reads a .pro file through loadThemeFile: GameFiles, not emulated); mode 1 = the
    negative control, which must read RED."""
    if not (portlib.has("resetGolfer") and portlib.has("updateMembership")):
        pytest.skip("resetGolfer (3.3) / updateMembership (3.2) not registered")
    portlib.negctl("customizeCharacter", mode)
    rng = random.Random(0x40F190)
    m = _m_customize(portlib)
    bad = []
    for k in range(12):
        image = bytes(m(rng, bytearray(_rand_image(portlib, rng))))
        why = _customize_compare(portlib, image, 0)
        if why:
            bad.append((k, why))
    if mode == 0:
        assert not bad, f"{len(bad)} of 12 differ, first {bad[:2]}"
    else:
        assert bad, "the negative control reads GREEN"


GOLDEN_NEGCTL = TERRAIN_DLL_NEGCTL + [("raiseCornerLevelsAround", 1, "test <="), ("lowerCornerLevelsAround", 1, "test >="),
                                      ("propagateType11", 1, "recursion everywhere"), ("rebuildHeightfield", 1, "flag 4")]


@need_exe
@pytest.mark.parametrize("function,mode,what", GOLDEN_NEGCTL)
def test_golden_digests_see_negctl(portlib, function, mode, what):
    """A wrong body must change at least one pinned digest (default runs: the golden scenarios reach the detail)."""
    if function not in GOLDEN:
        pytest.skip(f"{function}: no pinned digests")
    portlib.negctl(function, mode)
    assert _golden_port(portlib, function) != GOLDEN[function], f"{function} mode {mode} ({what}) is blind to the digests"


# =========================================================================================== footprints, edit queue, setup
# Functions whose vectors are one-sided, partial or absent, compared with the emulated original over random states
# (emu_terrain_call: playSound stubbed; the original's callees run in Unicorn, the port's are the ported ones, so a
# difference can also be a callee's).
def plain_compare(portlib, name, entry, image, args, void=False):
    ee, eimg, _, _, err = emu_terrain_call(image, [], entry, args, budget=2_000_000_000)
    with new_state(portlib) as st:
        st.load_image(image)
        try:
            pe = st.call(name, args).eax
            pimg = st.image()
        except L.SgError as e:
            return "" if ee is None else f"port error {e}"
    if ee is None:
        return f"emulated fault {err[:1]}"
    if pe != ee and not void:
        return f"eax port {pe:#x} emulated {ee:#x}"
    if pimg != eimg:
        i = next(i for i in range(len(pimg)) if pimg[i] != eimg[i])
        return f"GameState byte {i:#x} port {pimg[i]:#x} emulated {eimg[i]:#x}"
    return ""


def _m_course(portlib):
    """Placed objects with free slots, holes with par / length / tee / cup, an edit queue, camera words."""
    g = _m_golfers(portlib)
    o = objs(portlib)

    def m(rng, img):
        img = g(rng, img)
        recs = b""
        for i in range(256):
            t = rng.choice([-1, -1, -1, rng.randrange(20)]) if i else rng.choice([15, 15, 3])
            if 1 <= i < 32 and rng.random() < 0.5:
                t = 5                                   # lots: clearObjectFootprint's refund arm
            recs += struct.pack("<hhhBBii", t, rng.randrange(3, 44), rng.randrange(3, 44), rng.randrange(4),
                                rng.getrandbits(8), rng.randrange(0, 2000), rng.randrange(100))
        _set(portlib, img, "g_placed_objects", recs)
        holes = bytearray(20 * 0x208)
        for h in range(1, 19):
            r = h * 0x208
            holes[r] = rng.choice([0, 3, 4, 5])
            holes[r + 1] = rng.randrange(8)
            struct.pack_into("<h", holes, r + 4, rng.randrange(100, 700))
            struct.pack_into("<ii", holes, r + 8, rng.randrange(0, 48), rng.randrange(0, 48))
            struct.pack_into("<ii", holes, r + 0x18, rng.choice([0, rng.randrange(3, 46)]), rng.randrange(3, 46))
        _set(portlib, img, "g_holes", bytes(holes))
        n = rng.randrange(0, 8)
        _set(portlib, img, "unk_005a9cd4", struct.pack("<i", n))
        _set(portlib, img, "g_ui_55e920", struct.pack("<i", rng.randrange(0, n + 2)))
        _set(portlib, img, "g_editQueueX", struct.pack("<8i", *[rng.randrange(2, 47) for _ in range(8)]))
        _set(portlib, img, "g_editQueueY", struct.pack("<8i", *[rng.randrange(2, 47) for _ in range(8)]))
        modes = [rng.choice([0, 1, 2, 0x7E, 0x7F, 0x80 | 0x20, 0x100 | rng.randrange(4, 15), rng.randrange(0, 0x7D)])
                 for _ in range(8)]
        _set(portlib, img, "g_editQueueMode", struct.pack("<8i", *modes))
        _set(portlib, img, "g_cameraGoalX", struct.pack("<i", rng.choice([-1, -1, rng.randrange(50)])))
        _set(portlib, img, "g_holesPlusOne", struct.pack("<i", rng.randrange(1, 19)))
        _set(portlib, img, "g_year", struct.pack("<h", rng.randrange(0, 30)))
        _set(portlib, img, "g_tileBuildCode", bytes(rng.choice([0xFF, 0xFF, 0x80 | rng.randrange(0x14), rng.randrange(0x7F)])
                                                    for _ in range(2500)))
        _set(portlib, img, "g_tileRefund", bytes(rng.randrange(256) for _ in range(2500)))
        return img
    return m


def _m_flag(portlib, bit, then):
    def m(rng, img):
        img = then(rng, img)
        off = objs(portlib)["g_flags"][0]
        struct.pack_into("<I", img, off, struct.unpack_from("<I", img, off)[0] | bit)
        return img
    return m


def sim_cases(portlib):
    c = _m_course(portlib)
    return {
        "stampCourseTiles": (0x0040E000, [[_r(i, 1, 44), _r(i, 1, 44, 1), _r(i, 0, 20, 2), (-2, -1, 0, 0x20, 0x1000)[i % 5]]
                                          for i in range(30)], c, False),
        "clearObjectFootprint": (0x0040E400, [[_r(i, 0, 256)] if i % 2 else [_r(i, 1, 32)] for i in range(30)], c, True),
        "stepCourseEditQueue": (0x00409CF0, [[0] for _ in range(30)], c, False),
        "demolishObject": (0x0040A4E0, [[_r(i, 0, 50), _r(i, 0, 50, 1), 0] for i in range(40)], c, False),
        "buildTournamentPrep": (0x0046D200, [[0] for _ in range(12)], _m_flag(portlib, 0x4000000, c), False),
        "buildGolferBioText": (0x004315E0, [[0] for _ in range(6)], c, False),
    }


SIM_ORACLE = ["stampCourseTiles", "clearObjectFootprint", "stepCourseEditQueue", "demolishObject", "buildTournamentPrep",
              "buildGolferBioText"]
SIM_ORACLE_NEGCTL = [("stampCourseTiles", 1, "corner cell takes the level"), ("stampCourseTiles", 2, "kind 0xa marks"),
                     ("clearObjectFootprint", 1, "finance year untouched"), ("stepCourseEditQueue", 1, "camera glide step 2"),
                     ("demolishObject", 1, "finance year untouched"), ("buildTournamentPrep", 1, "orientation 1"),
                     ("buildGolferBioText", 1, "ball records keep x"), ("buildGolferBioText", 2, "colour range 9")]


def _sim_run(portlib, function, first=False):
    entry, argsets, mutate, void = sim_cases(portlib)[function]
    rng = random.Random(zlib.crc32(b"sim" + function.encode()))
    bad, faults = [], 0
    for k, args in enumerate(argsets):
        image = bytes(mutate(rng, bytearray(_rand_image(portlib, rng))))
        why = plain_compare(portlib, function, entry, image, args, void)
        if why:
            bad.append((k, args, why))
            if first:
                break
    return bad, len(argsets)


@emu_oracle
@need_exe
@pytest.mark.parametrize("function", SIM_ORACLE)
def test_sim_rows_equal_emulated_original(portlib, function):
    bad, n = _sim_run(portlib, function)
    assert not bad, f"{function}: {len(bad)} of {n} differ, first {bad[:2]}"


@emu_oracle
@need_exe
@pytest.mark.parametrize("function,mode,what", SIM_ORACLE_NEGCTL)
def test_sim_rows_negctl_reads_red(portlib, function, mode, what):
    portlib.negctl(function, mode)
    bad, n = _sim_run(portlib, function, first=True)
    assert bad, f"{function} mode {mode} ({what}) reads GREEN against the emulated original"
