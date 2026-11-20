"""P39B play switch layPath.underEveryType (1 = the original; play profile 0; log/roadmap/P39/P39B_notes.md).

Two halves, each judged at 1 and at 0:

- the Terrain-state draw rule (terrain_state.cpp setPathFlagForType / layPath / enterType7 / setTypeId): the recorded Terrain call
  log of corpus terrain/pin_cov_b_paths (a course where paths are laid across several types) is replayed from an empty object at
  both settings. At 1 every path tile of type tee, green, fairway, firm fairway, sand, pot bunker or large building has the
  path-draw byte (+0x20a) clear (the original's rule, which test_terrain_state judges against the original's snapshots). At 0 the
  byte is set on every path tile except tee / green (and water / out of bounds, kept as before), and no other byte of the object
  differs from the run at 1.
- the texture flood (terrain_dll.cpp path_tex / terrain.cpp path_texture_reach): a course built here (clubhouse, one path from it
  across rough, a fairway tile and a green tile, back onto rough, and a second path not joined to it) goes through the port's
  rebuildTileCoverage then rebuildTerrainGrid with the recording fake Terrain (terrain_fake.cpp) behind the boundary. The layPath
  calls' texture argument (bit 1 << 6) must be the original's TF_FLOOD_REACHED at 1 (set up to the fairway tile, clear past it)
  and set on the whole joined path at 0; the unjoined path keeps 0 at both; GameState is byte-identical between the two runs
  (TF_FLOOD_REACHED, the placed objects' joined bit: nothing written by the switch).

    ~/bin/py -3.12 -m pytest port/test/test_p39b_paths.py -q
"""
from __future__ import annotations

import ctypes as C
import random
import struct

import pytest

from port.test import lib as L
from port.test import terraincorpus as TC
from port.test import test_terrain as T
from port.test import test_terrain_state as TS

pytestmark = pytest.mark.port

SWITCH = "layPath.underEveryType"
ROOT = TC.CORPUS / "pin_cov_b_paths"
TILES, TILE = 0x3A4, 0x248
TYPE, HAS, CONN, DRAW = 0x24, 0x208, 0x209, 0x20A
HIDDEN_AT_1 = {0, 1, 2, 3, 7, 9, 0x16}   # setPathFlagForType / layPath / enterType7's clear list (water / out of bounds aside)
KEPT = {0x11, 0x14}                      # water and out of bounds: setPathFlagForType's rule is kept at 0


@pytest.fixture(autouse=True)
def _reset(portlib):
    portlib.defect_reset()
    yield
    portlib.defect_reset()


# ------------------------------------------------------------------------------------------------ the draw rule (replay)
PROBE_TYPES = (0, 1, 2, 3, 7, 9, 0x16, 4)   # tee, green, fairway, firm fairway, sand, pot bunker, large building, rough
# the path-draw byte after setType(tile, type, 0) and after layPath(tile, 1, 0) on a path tile: the original (1) and the switch at 0.
# At 1 setTypeId's cases 0 and 9 clear it through resetFaceTextures, 7 through enterType7, the others through setPathFlagForType.
EXPECT = {1: ({0: 0, 1: 0, 2: 0, 3: 0, 7: 0, 9: 0, 0x16: 0, 4: 1}, {0: 0, 1: 0, 2: 0, 3: 0, 7: 0, 9: 0, 0x16: 0, 4: 1}),
          0: ({0: 0, 1: 0, 2: 1, 3: 1, 7: 1, 9: 1, 0x16: 1, 4: 1}, {0: 0, 1: 0, 2: 1, 3: 1, 7: 1, 9: 1, 0x16: 1, 4: 1})}


def _replay(portlib, at: int, probe: bool = False):
    """The object after the whole log at switch value `at`; with probe, also {type: (byte after setType, after layPath)} from
    repainting one replayed path tile per PROBE_TYPES entry (the real Terrain-state calls, through sg_ts_call)."""
    if not (ROOT / "calls.txt").exists():
        pytest.skip(f"corpus {ROOT} is not present")
    portlib.defect(SWITCH, bool(at))
    log = TC.calls(ROOT)
    end = max(c.seq for c in log) + 1
    base = log[0].ret
    got = {}

    def call(st, name, args):
        a = (C.c_uint32 * 5)(*(list(args) + [0] * (5 - len(args))))
        r = C.c_uint32(0)
        assert portlib.dll.sg_ts_call(st.ctx, name.encode(), base, a, None, 0, C.byref(r)) == 0, name

    def on_stop(_key, st, obj):
        got["obj"] = bytes(obj)
        if not probe:
            return
        tiles = [k for k, ty, has, _c, _d in _tiles(got["obj"]) if has and ty not in KEPT]
        assert len(tiles) >= len(PROBE_TYPES), "too few replayed path tiles to probe"
        res = {}
        for k, ty in zip(tiles, PROBE_TYPES):
            t = TILES + k * TILE
            call(st, "setType", [base + t, ty, 0])
            after_set = obj[t + DRAW]
            call(st, "layPath", [base + t, 1, 0])
            res[ty] = (after_set, obj[t + DRAW])
        got["probe"] = res

    out = TS.run_log(portlib, ROOT, [(end, "end")], on_stop)
    assert "_error" not in out, out["_error"]
    return (got["obj"], got["probe"]) if probe else got["obj"]


def _tiles(obj: bytes):
    w, h = struct.unpack_from("<ii", obj, 0x14)
    for k in range(w * h):
        t = TILES + k * TILE
        yield k, struct.unpack_from("<i", obj, t + TYPE)[0], obj[t + HAS], obj[t + CONN], obj[t + DRAW]


def test_path_drawn_under_every_type_but_tee_and_green(portlib):
    a, b = _replay(portlib, 1), _replay(portlib, 0)
    seen, changed = {}, 0
    for (k, ty, has, conn, d1), (_, ty0, has0, conn0, d0) in zip(_tiles(a), _tiles(b)):
        assert (ty, has, conn) == (ty0, has0, conn0), f"tile {k}: type / path / texture bytes differ between 1 and 0"
        if not has:
            continue
        seen[ty] = seen.get(ty, 0) + 1
        if ty in HIDDEN_AT_1:
            assert d1 == 0, f"tile {k} type {ty:#x}: drawn at 1 (the original clears it)"
        if ty in KEPT:
            assert d0 == d1, f"tile {k} type {ty:#x}: water / out of bounds rule changed"
        elif ty in (0, 1):
            assert d0 == 0, f"tile {k} type {ty:#x}: a path drawn on a tee / green at 0"
        else:
            assert d0 == 1, f"tile {k} type {ty:#x}: a path not drawn at 0"
        changed += d0 != d1
    hidden = sorted(t for t in seen if t in HIDDEN_AT_1 - {0, 1})
    assert hidden, f"the corpus has no path on a type the switch changes: {seen}"
    assert changed > 0
    diff = [i for i in range(len(a)) if a[i] != b[i] and (i - TILES) % TILE != DRAW]
    assert not diff, f"bytes other than the path-draw byte differ: first {hex(diff[0])}"
    print(f"path tiles by type {dict(sorted(seen.items()))}; path-draw byte changed on {changed}")


@pytest.mark.parametrize("at", (1, 0))
def test_repainted_path_tiles(portlib, at):
    """setType then layPath on replayed path tiles, one per type: the byte each leaves is the original's at 1, the new rule at 0."""
    _obj, probe = _replay(portlib, at, probe=True)
    want_set, want_lay = EXPECT[at]
    bad = {ty: v for ty, v in probe.items() if v != (want_set[ty], want_lay[ty])}
    assert not bad, f"switch {at}: (after setType, after layPath) by type differ: {bad}"


# ------------------------------------------------------------------------------------------------ the texture flood (fake)
N = 50
CLUB = (10, 10)
JOINED = [(10, y) for y in range(10, 31)]     # from the clubhouse's corner tile along y
CROSS = {(10, 20): 2, (10, 21): 1}            # a fairway tile, then a green tile, on the path
LOOSE = [(30, y) for y in range(30, 36)]      # a path not joined to the clubhouse


def _course(portlib) -> tuple[bytes, list]:
    o = T.objs(portlib)
    assert portlib.exe_load() == ""
    with portlib.state() as st:
        st.fill_from_exe(whole_image=True)
        img = bytearray(st.image())

    def put(name, data):
        off, size = o[name]
        assert len(data) <= size, name
        img[off:off + len(data)] = data

    init = o["f_tab_tileTypeAttrInit_4c1a40"]
    put("g_tileTypeAttrs", bytes(img[init[0]:init[0] + 23 * 0x30]))
    types = bytearray([4] * (N * N))          # rough
    flags = [0] * (N * N)
    for (x, y) in JOINED + LOOSE:
        flags[x * N + y] |= 1 << 5
    for (x, y), t in CROSS.items():
        types[x * N + y] = t
    put("g_tileType", bytes(types))
    put("g_tileFlags", struct.pack(f"<{N * N}H", *flags))
    put("g_tileVariant", bytes(N * N))
    put("g_cornerLevel", bytes([3] * ((N + 1) * (N + 1))))
    recs = struct.pack("<hhhBBii", 15, CLUB[0], CLUB[1], 0, 0, 0, 0)
    recs += struct.pack("<hhhBBii", -1, 0, 0, 0, 0, 0, 0) * 255
    put("g_placed_objects", recs)
    put("g_difficulty", struct.pack("<i", 1))
    put("g_flags", struct.pack("<I", 0))
    image, blocks = T._ter_setup(portlib, random.Random(39), img, heights=(0, 1))
    return bytes(image), blocks


def _run(portlib, at: int):
    portlib.defect(SWITCH, bool(at))
    image, blocks = _course(portlib)
    with T.new_state(portlib) as st:
        st.load_image(image)
        bufs = []
        for base, b in blocks:
            buf = (C.c_uint8 * len(b)).from_buffer_copy(bytes(b))
            st.map(base, buf)
            bufs.append(buf)
        log = (C.c_uint8 * (4 + T.LOG_CAP * 32))()
        st.map(T.LOG_VA, log)
        st.call("terrainFake::install", [T.LOG_VA, T.LOG_CAP, T.HEAP_VA + 0x100, T.HEAP_SIZE - 0x100, 6])
        st.call("rebuildTileCoverage", [0])
        st.call("rebuildTerrainGrid", [])
        st.call("terrainFake::remove", [])
        final = st.image()
        raw = bytes(log)
    n = struct.unpack_from("<I", raw, 0)[0]
    assert n <= T.LOG_CAP
    tex = {}
    for k in range(n):
        rec = struct.unpack_from("<8I", raw, 4 + 32 * k)
        if rec[0] == 5:   # kLayPath {tile, f20, f40}
            i = (rec[1] - T.TERR_VA - T.TILES_AT) // T.TILE_SIZE
            tex[(i % N, i // N)] = rec[3]   # the Terrain's tile index is x + y * width
    return tex, final


@T.need_exe
def test_paved_texture_follows_the_path_across_fairway_and_green(portlib):
    t1, img1 = _run(portlib, 1)
    t0, img0 = _run(portlib, 0)
    assert set(t1) == set(t0) == set(JOINED) | set(LOOSE), "layPath calls on other tiles than the path"
    past = [p for p in JOINED if p[1] > 20]
    assert all(t1[p] == 1 << 6 for p in JOINED if p[1] <= 20), "the original's flood should reach up to the fairway tile"
    assert all(t1[p] == 0 for p in past), "at 1 the texture past the fairway is the original's (dirt)"
    assert all(t0[p] == 1 << 6 for p in JOINED), "at 0 the whole joined path takes the paved texture"
    assert all(t1[p] == t0[p] == 0 for p in LOOSE), "a path not joined to the clubhouse stays dirt"
    assert img1 == img0, "the switch wrote GameState (TF_FLOOD_REACHED / joined bits must stay the original's)"


# ------------------------------------------------------------------------------------------------ negative controls
def test_negctl_terrain_state_24_reads_red(portlib):
    """terrain_state.cpp mode 24: at 0 a fairway still clears the path-draw byte; the replay must see a fairway path tile undrawn."""
    TS.bind(portlib.dll)
    portlib.dll.sg_ts_negctl(24)
    try:
        _obj, probe = _replay(portlib, 0, probe=True)
    finally:
        portlib.dll.sg_ts_negctl(0)
    assert probe[2] != (EXPECT[0][0][2], EXPECT[0][1][2]), "mode 24 reads GREEN on the repainted fairway tile"


@T.need_exe
def test_negctl_toggle_path_2_reads_red(portlib):
    """terrain_dll.cpp togglePath mode 2: the texture flood stops like floodMark, so the path past the fairway is dirt again."""
    portlib.negctl("togglePath", 2)
    try:
        t0, _ = _run(portlib, 0)
    finally:
        portlib.negctl("togglePath", 0)
    assert any(t0[p] == 0 for p in JOINED if p[1] > 20)
