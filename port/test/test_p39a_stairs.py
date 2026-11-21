"""P39A2 play switch layPath.stairs (1 = the original; play profile 0; re/roadmap/P39_COURSE_FEATURES.md "P39A decided",
log/roadmap/P39/P39A2_notes.md).

A path tile whose path axis rises 3 or more levels is stairs. Computed from the heights, nothing stored (terrain.h
path_stairs_axes): the axis is x or y when a path tile lies next to it along that axis, and the rise is |slopeMix| along it at
the tile's four quarter points (the 0x2b "Must I walk up this steep slope?" test of updateGolfers, > 2). Judged here:

- detection: stairs.axes equals a Python reference built on the registered slopeMix over synthetic courses at N = 50 and 101
  (a ramp along x of 3 levels a tile, one of 2, one along y of 4; a path along each ramp, across it, a lone tile, a junction), 0
  everywhere at 1, and no GameState write;
- price: stairs.price doubles the price on a stairs tile (the tile assumed laid), keeps it elsewhere and at 1; the path tool's
  frame (mainLoop H2_Build case 0, corpus segment cov_b_paths/00 with the ground under its paths turned into a ramp) charges twice
  on the stairs tiles and the original's price on the others, and at 1 the run is the recorded one;
- the thought: one updateGolfers call (mood5_fixture/00) with a walking golfer on a path tile of a ramp rising 3 levels in its
  walking direction emits 0x2b at 1 and not at 0;
- the drawing (terrain_render.cpp draw_paths, the Terrain path pass of corpus g3, cov_b_paths): with the sim grids making one path
  tile stairs and the Terrain vertices on the same ramp, the tile's bands along the axis become four low treads per level crossed
  (P39A2b: a riser of a quarter level, 12 on a 3-level tile; flat, on the riser lattice, evenly spaced, the top one level with the
  path beyond the span; path texture) with retaining-wall risers, at every view; every other tile's calls are unchanged, and at 1
  the stream equals the run without the ramp's stairs.
Negative controls: stairs.axes mode 1 (4 levels) and mode 2 (no path-axis test) read RED on the reference; terrain_render 20 (one
tread per span), 21 (steps drawn near to far), 22 (faces turned away from the camera drawn too) and 23 (P39A2's one step per
level) read RED on the drawing checks.
- the drawing from 2 levels (P39A2b, Mariano's pick): a path tile rising 2 levels is no sim stairs (stairs.axes 0: no x2 price, no
  thought change) but is drawn as 8 low treads (3.75 .. 30); control terrain_render 24 (only the sim's stairs drawn) leaves the band.

    ~/bin/py -3.12 -m pytest port/test/test_p39a_stairs.py -q
"""
from __future__ import annotations

import ctypes as C
import struct

import pytest

from port.test.test_terrain import need_exe, new_state, objs
from port.test.test_terrain_gen import _fixed_image

pytestmark = pytest.mark.port

SWITCH = "layPath.stairs"
PATH = 1 << 5
ROUGH = 4


@pytest.fixture(autouse=True)
def _reset(portlib):
    portlib.defect_reset()
    portlib.negctl_reset()
    yield
    portlib.defect_reset()
    portlib.negctl_reset()


def _s32(v):
    return v - (1 << 32) if v & 0x80000000 else v


# ---------------------------------------------------------------------------------------------------- a synthetic course
def ramps(n):
    """corner levels (N + 1)^2 (x * (N + 1) + y) with three ramps, types (all rough) and path flags; returns (levels, types,
    flags, named tiles)"""
    v = n + 1
    lv = bytearray([4]) * (v * v)
    s = n / 50.0
    p = lambda a: int(round(a * s))  # noqa: E731

    def ramp_x(x0, y0, y1, per, tiles):   # levels rise `per` a tile along x from lattice column x0, for lattice rows y0..y1
        for lx in range(x0, x0 + tiles + 1):
            for ly in range(y0, y1 + 1):
                lv[lx * v + ly] = 4 + per * (lx - x0)
        for lx in range(x0 + tiles + 1, n + 1):
            for ly in range(y0, y1 + 1):
                lv[lx * v + ly] = 4 + per * tiles

    def ramp_y(y0, x0, x1, per, tiles):
        for ly in range(y0, y0 + tiles + 1):
            for lx in range(x0, x1 + 1):
                lv[lx * v + ly] = 4 + per * (ly - y0)
        for ly in range(y0 + tiles + 1, n + 1):
            for lx in range(x0, x1 + 1):
                lv[lx * v + ly] = 4 + per * tiles

    A = (p(10), p(10))     # ramp A: +3 a tile along x, x 10..13, rows 6..16
    ramp_x(A[0], p(6), p(16), 3, 3)
    B = (p(10), p(30))     # ramp B: +2 a tile along x
    ramp_x(B[0], p(26), p(36), 2, 3)
    Cr = (p(30), p(30))    # ramp C: +4 a tile along y, y 30..33, columns 26..36
    ramp_y(Cr[1], p(26), p(36), 4, 3)
    T = bytearray([ROUGH]) * (n * n)
    F = [0] * (n * n)

    def path(x, y):
        F[x * n + y] |= PATH
    named = {}
    for x in range(A[0] - 1, A[0] + 5):           # along ramp A (x axis): stairs on the ramp's tiles
        path(x, A[1])
    for y in range(A[1] + 2, A[1] + 5):           # across ramp A (y axis) on one ramp column: not stairs
        path(A[0] + 1, y)
    path(A[0] + 2, A[1] + 5)                      # a two-tile path along x on the ramp, away from that column: stairs
    path(A[0] + 3, A[1] + 5)
    path(A[0] + 2, A[1] - 3)                      # a lone tile on the ramp: no path neighbour
    for x in range(B[0] - 1, B[0] + 5):           # along ramp B: 2 levels a tile, not stairs
        path(x, B[1])
    for y in range(Cr[1] - 1, Cr[1] + 5):         # along ramp C (y axis): stairs
        path(Cr[0], y)
    for x in range(Cr[0] - 1, Cr[0] + 2):         # a junction on ramp C: x neighbours as well
        path(x, Cr[1] + 1)
    named["along_x"] = (A[0] + 1, A[1])
    named["across"] = (A[0] + 1, A[1] + 3)
    named["beside"] = (A[0] + 2, A[1] + 5)
    named["lone"] = (A[0] + 2, A[1] - 3)
    named["gentle"] = (B[0] + 1, B[1])
    named["along_y"] = (Cr[0], Cr[1] + 2)
    named["junction"] = (Cr[0], Cr[1] + 1)
    named["flat"] = (A[0] - 1, A[1])
    return bytes(lv), bytes(T), F, named


def _state(portlib, n):
    st = new_state(portlib).__enter__()
    img = bytearray(_fixed_image(portlib))
    o = objs(portlib)
    img[o["g_date"][0]:o["g_date"][0] + 4] = struct.pack("<i", 0x4B0C)   # heightBlend reads g_cornerLevel once g_date != 0
    st.load_image(bytes(img))
    if n != 50:
        st.world_resize(n)
    lv, T, F, named = ramps(n)
    st.mem_write(st.grid_va("g_tileType"), T)
    st.mem_write(st.grid_va("g_tileFlags"), struct.pack(f"<{n * n}H", *F))
    st.mem_write(st.grid_va("g_cornerLevel"), lv)
    st.mem_write(st.grid_va("g_cornerCache"), bytes(8 * n * n))   # cornerHeights reads the cache first when it is non-zero
    return st, F, named


def ref_axes(st, F, n, x, y, need=3, axis_test=True, dx_tab=None, dy_tab=None, assume=False):
    """the rule, from the registered slopeMix"""
    path = lambda a, b: 0 <= a < n and 0 <= b < n and F[a * n + b] & PATH  # noqa: E731
    if not assume and not path(x, y):
        return 0
    axes = 0
    for d in (0, 2, 4, 6):
        dx, dy = dx_tab[d], dy_tab[d]
        bit = 1 if dx else 2
        if axes & bit:
            continue
        if axis_test and not path(x + dx, y + dy) and not path(x - dx, y - dy):
            continue
        rise = 0
        for q in range(4):
            wx = (x << 10) + (0x300 if q & 1 else 0x100)
            wy = (y << 10) + (0x300 if q & 2 else 0x100)
            r = _s32(st.call("slopeMix", [wx, wy, d]).eax & 0xFFFFFFFF)
            rise = max(rise, abs(r))
        if rise >= need:
            axes |= bit
    return axes


def _tabs(portlib, st):
    o = objs(portlib)
    dx = struct.unpack("<8i", st.read(o["f_tab_dirDx_4c2878"][0], 32))
    dy = struct.unpack("<8i", st.read(o["f_tab_dirDy_4c2898"][0], 32))
    return dx, dy


def _axes(st, x, y, assume=0):
    return st.call("stairs.axes", [x, y, assume]).eax


def _region(named, n):
    xs = [p[0] for p in named.values()]
    ys = [p[1] for p in named.values()]
    return [(x, y) for x in range(max(0, min(xs) - 3), min(n, max(xs) + 6)) for y in range(max(0, min(ys) - 5), min(n, max(ys) + 6))]


# ---------------------------------------------------------------------------------------------------- tests: detection
def test_switch_registered_default_original(portlib):
    d = portlib.defects()
    assert SWITCH in d and d[SWITCH]["on"] == 1


@need_exe
@pytest.mark.parametrize("n", [50, 101])
def test_detection_equals_reference(portlib, n):
    portlib.defect(SWITCH, False)
    st, F, named = _state(portlib, n)
    try:
        dx, dy = _tabs(portlib, st)
        img0 = st.image()
        for (x, y) in _region(named, n):
            assert _axes(st, x, y) == ref_axes(st, F, n, x, y, dx_tab=dx, dy_tab=dy), (x, y)
            assert _axes(st, x, y, 1) == ref_axes(st, F, n, x, y, dx_tab=dx, dy_tab=dy, assume=True), (x, y, "assume")
        want = {"along_x": 1, "across": 0, "beside": 1, "lone": 0, "gentle": 0, "along_y": 2, "junction": 2, "flat": 0}
        got = {k: _axes(st, *p) for k, p in named.items()}
        assert got == want, got
        # the rise on ramp A is 3 a tile: slopeMix says so where the rule looks (a fixture check, not the port's rule)
        x, y = named["along_x"]
        assert max(abs(_s32(st.call("slopeMix", [(x << 10) + 0x100, (y << 10) + 0x100, d]).eax & 0xFFFFFFFF)) for d in (2, 6)) == 3
        assert st.image() == img0, "stairs.axes wrote GameState"
        portlib.defect(SWITCH, True)
        assert all(_axes(st, x, y) == 0 and _axes(st, x, y, 1) == 0 for (x, y) in _region(named, n))
    finally:
        st.__exit__(None, None, None)


@need_exe
@pytest.mark.parametrize("mode,tile,want", [(1, "along_x", 0), (2, "across", 1), (2, "lone", 1)])
def test_detection_controls_read_red(portlib, mode, tile, want):
    """mode 1 (4 levels) loses ramp A's stairs; mode 2 (no path-axis test) makes the tile across the ramp and the lone tile
    stairs: both differ from the rule test_detection_equals_reference holds"""
    portlib.defect(SWITCH, False)
    st, F, named = _state(portlib, 50)
    try:
        assert portlib.negctl("stairs.axes", mode) == 0
        assert _axes(st, *named[tile]) == want
    finally:
        st.__exit__(None, None, None)


@need_exe
def test_price_doubles_on_stairs(portlib):
    portlib.defect(SWITCH, False)
    st, F, named = _state(portlib, 50)
    try:
        price = lambda k, p=7, a=1: _s32(st.call("stairs.price", [*named[k], p, a]).eax & 0xFFFFFFFF)  # noqa: E731
        assert price("along_x") == 14 and price("along_y") == 14 and price("junction") == 14
        assert price("across") == 7 and price("gentle") == 7 and price("flat") == 7 and price("lone") == 7
        assert price("along_x", 15, 0) == 30   # terrain tool 0xa0's water price, the flag already set
        # a tile not yet laid next to ramp A's path: assumed laid it is stairs
        x, y = named["along_x"]
        st.mem_write(st.grid_va("g_tileFlags") + 2 * (x * 50 + y), struct.pack("<H", 0))
        assert _axes(st, x, y) == 0 and _axes(st, x, y, 1) == 1
        assert _s32(st.call("stairs.price", [x, y, 7, 1]).eax & 0xFFFFFFFF) == 14
        assert _s32(st.call("stairs.price", [x, y, 7, 0]).eax & 0xFFFFFFFF) == 7
        portlib.defect(SWITCH, True)
        assert _s32(st.call("stairs.price", [x, y, 7, 1]).eax & 0xFFFFFFFF) == 7
    finally:
        st.__exit__(None, None, None)


# ---------------------------------------------------------------------------------------------------- tests: the 0x2b thought
DX8 = (0, 1, 1, 1, 0, -1, -1, -1)   # f_tab_dirDx_4c2878 / f_tab_dirDy_4c2898 (direction8's octants, as test_golfers_p36b)
DY8 = (-1, -1, 0, 1, 1, 1, 0, -1)
THOUGHT = 0x2B


def _golfer_case(portlib, switch, g, ramp, rec_out=None):
    """one updateGolfers call on mood5_fixture/00 with a plus of path tiles around golfer g's tile; with `ramp` = (axis, sign) the
    corner levels around it rise 3 a tile along that axis in that sign (levels 3 .. 15). Returns (thoughts list +0x70..+0x79 before, after, the
    walking direction +0x1a after, the tile after, stairs.axes of that tile)."""
    from port.test.test_defects_p28 import _seg_image
    from port.test.test_golfers import VL
    img = _seg_image("mood5_fixture", "00")
    portlib.defect(SWITCH, switch)
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
            i = 2 * ((tx + ax) * n + ty + ay)
            fl[i] |= PATH
        st.write(o["g_tileFlags"], bytes(fl))
        if ramp is not None:
            axis, sign = ramp
            v = n + 1
            lv = bytearray(st.read(o["g_cornerLevel"], v * v))
            for lx in range(tx - 2, tx + 4):          # the lattice of tiles tx - 2 .. tx + 2 (tile x has corners x, x + 1)
                for ly in range(ty - 2, ty + 4):
                    t = (lx - tx) if axis == 0 else (ly - ty)
                    lv[lx * v + ly] = 9 + 3 * sign * max(-2, min(2, t))
            st.write(o["g_cornerLevel"], bytes(lv))
            st.write(o["g_cornerCache"], bytes(8 * n * n))
        st.write(r + 0x14, struct.pack("<h", 0))   # a new step this call (updateGolfers+0x28ec: the step counter <= 0)
        before = st.read(r + 0x70, 10)
        st.call("updateGolfers", [0])
        after = st.read(r + 0x70, 10)
        d = st.read(r + 0x1a, 1)[0] & 7
        x2, y2 = struct.unpack("<ii", st.read(r, 8))
        portlib.defect(SWITCH, False)
        axes = st.call("stairs.axes", [x2 >> 10, y2 >> 10, 0]).eax
        return before, after, d, (x2 >> 10, y2 >> 10), axes


def _walker_on_flat(portlib):
    """a walking golfer of mood5_fixture/00 whose step (with the plus of paths) is orthogonal; (g, its direction)"""
    from port.test.test_defects_p28 import _seg_image
    from port.test.test_golfers_p36b import _walkers
    img = _seg_image("mood5_fixture", "00")
    o = {nm: v for nm, v, _ in portlib.objects()}
    for g in _walkers(lambda k: img[o["g_golfers"] + k * 0x100:][:0x100]):
        _b, _a, d, _t, _ax = _golfer_case(portlib, True, g, None)
        if d % 2 == 0:
            return g, d
    pytest.skip("no walking golfer with an orthogonal step in mood5_fixture/00")


@need_exe
def test_golfer_on_stairs_skips_the_steep_slope_thought(portlib):
    g, d = _walker_on_flat(portlib)
    axis = 0 if DX8[d] != 0 else 1
    sign = DX8[d] if axis == 0 else DY8[d]
    b1, a1, d1, t1, ax1 = _golfer_case(portlib, True, g, (axis, sign))     # the original: uphill 3 levels on a path tile
    b0, a0, d0, t0, ax0 = _golfer_case(portlib, False, g, (axis, sign))
    assert d1 == d and d0 == d and t1 == t0, (d, d1, d0, t1, t0)           # the heights change nothing of the walk
    assert a1[0] == THOUGHT and a1[1:] == b1[:9], (b1.hex(), a1.hex())     # 0x2b emitted at 1
    assert ax0 == (1 if axis == 0 else 2), ax0                              # the tile is stairs along the walk
    assert THOUGHT not in a0[:1] and a0 == b0, (b0.hex(), a0.hex())        # and skipped at 0 (no other thought either)
    # control: going down the same ramp the original emits nothing either (the rule is the climb), so the skip is the stairs'
    b2, a2, *_ = _golfer_case(portlib, True, g, (axis, -sign))
    assert a2 == b2


# ---------------------------------------------------------------------------------------------------- tests: the drawing
# the Terrain path pass (pathUpdateRender, pivot tile (37, 19)) of corpus g3 (cov_b_paths) at the four views; its tiles (34, 17)
# (a path along x) and (37, 19) (a path along y) are put on a ramp of 3 levels: the Terrain vertices and the sim's corner levels
PASSES = {0: "i002_paths_pathupd", 1: "i003_paths_pathupd", 2: "i004_paths_pathupd", 3: "i005_paths_pathupd"}
STEP = 15.0                      # the corner step one Terrain level adds (TerrainGlobals::corner_step, elevate / lowerCorner)
RISE = STEP / 4                  # P39A2b: terrain_render.cpp kStairStepsPerLevel = 4 low steps a level
TREADS = [round(RISE * i, 3) for i in range(1, 13)]   # a 3-level tile: 12 treads, 3.75 .. 45
TX, TY = (34, 17), (37, 19)      # a straight x run tile and a straight y run tile (path neighbours on that axis only)
TILES, TS = 0x3A4, 0x248


def _f(h):
    return struct.unpack("<f", struct.pack("<I", int(h, 16)))[0]


def _fans(lines):
    """the immediate-mode primitives of a call stream: (texture bound, [normal], [(s, t)], [(x, y, z)]) per glBegin .. glEnd"""
    out, tex, cur = [], None, None
    for ln in lines:
        name, _, args = ln.partition(" ")
        if name == "glBindTexture":
            tex = int(args.split(",")[1], 16)
            continue
        if name == "glBegin":
            cur = (tex, [], [], [])
            continue
        if name == "glEnd":
            if cur is not None:
                out.append(cur)
            cur = None
            continue
        if cur is None or not args:
            continue
        vals = tuple(_f(v) for v in args.replace("v=", "").split(","))
        if name in ("glNormal3fv", "glNormal3f"):
            cur[1].append(vals)
        elif name in ("glTexCoord2f", "glTexCoord2fv"):
            cur[2].append(vals)
        elif name in ("glVertex3f", "glVertex3fv"):
            cur[3].append(vals)
    return out


def _render(portlib, view, stairs_on, ramp="x", negctl=0, levels=3):
    """the pass's call lines with the ramp of `levels` levels under TX (ramp "x") or TY ("y") or none (None): the Terrain vertices
    and the sim grids; returns (lines, info)"""
    from port.test import glcorpus as G
    G.bind(portlib.dll)
    snap = G.Snap(G.CORPUS / "g3", PASSES[view])
    portlib.defect(SWITCH, not stairs_on)
    n = 50
    with new_state(portlib) as st:
        o = objs(portlib)
        st.write(o["g_date"][0], struct.pack("<i", 0x4B0C))
        tb = snap.tobj
        w = struct.unpack_from("<i", tb, 0x14)[0]
        flags = [0] * (n * n)
        for i in range(w * w):
            x, y = struct.unpack_from("<ii", tb, TILES + i * TS + 0x2C)
            if tb[TILES + i * TS + 0x208]:
                flags[x * n + y] |= PATH
        st.mem_write(st.grid_va("g_tileType"), bytes([ROUGH]) * (n * n))
        st.mem_write(st.grid_va("g_tileFlags"), struct.pack(f"<{n * n}H", *flags))
        v = n + 1
        lv = bytearray([4]) * (v * v)
        vert = list(snap.arr(G.RVA["vertices"], G.N_VERTICES * 3, C.c_float))
        row = w * 3
        info = {"base": {}}
        if ramp == "x":
            x, y = TX                                # the corners of tile (x, y): lattice columns x, x + 1, rows y - 1, y
            for lx in range(x + 1, n + 1):
                for ly in (y - 1, y):
                    lv[lx * v + ly] = 4 + levels
        elif ramp == "y":
            x2, y2 = TY
            for ly in range(y2, n + 1):              # tile (x2, y2): rows y2 - 1 (low), y2 (high)
                for lx in (x2, x2 + 1):
                    lv[lx * v + ly] = 4 + levels
        if ramp:

            def setv(tx, ty, fn):                    # the 3 x 3 vertices of a tile: index tx * 3 + i + (ty * 3 + j) * row
                for i in range(3):
                    for j in range(3):
                        k = tx * 3 + i + (ty * 3 + j) * row
                        vert[3 * k + 1] = fn(i, j)
            for (tx, ty), along in ((TX, 0),) if ramp == "x" else ((TY, 1),):
                c = tx * 3 + 1 + (ty * 3 + 1) * row
                h0 = round(vert[3 * c + 1] / STEP) * STEP
                info["base"][(tx, ty)] = h0
                if along == 0:
                    setv(tx - 1, ty, lambda i, j, h0=h0: h0)
                    setv(tx, ty, lambda i, j, h0=h0: h0 + levels * STEP / 2 * i)
                    setv(tx + 1, ty, lambda i, j, h0=h0: h0 + levels * STEP)
                else:
                    setv(tx, ty - 1, lambda i, j, h0=h0: h0)
                    setv(tx, ty, lambda i, j, h0=h0: h0 + levels * STEP / 2 * j)
                    setv(tx, ty + 1, lambda i, j, h0=h0: h0 + levels * STEP)
        st.mem_write(st.grid_va("g_cornerLevel"), bytes(lv))
        st.mem_write(st.grid_va("g_cornerCache"), bytes(8 * n * n))
        if stairs_on:
            info["axes"] = {t: st.call("stairs.axes", [*t, 0]).eax for t in (TX, TY)}
        st.map(snap.terrain, (C.c_uint8 * len(snap.tobj)).from_buffer_copy(snap.tobj))
        g = G.SgGlGlobals()
        keep = [(C.c_float * len(vert))(*vert), snap.arr(G.RVA["normals"], G.N_VERTICES * 3, C.c_float),
                snap.arr(G.RVA["textures"], G.N_TEXTURES, C.c_uint32),
                snap.arr(G.RVA["tex_coords"], G.N_TEXCOORD_FLOATS, C.c_float), snap.arr(G.RVA["side_normals"], 24, C.c_float)]
        g.vertices, g.n_vertices = C.addressof(keep[0]), G.N_VERTICES
        g.normals, g.n_normals = C.addressof(keep[1]), G.N_VERTICES
        g.textures, g.n_textures = C.addressof(keep[2]), G.N_TEXTURES
        g.tex_coords, g.n_tex_coords = C.addressof(keep[3]), G.N_TEXCOORD_FLOATS
        g.side_normals = C.addressof(keep[4])
        g.wall_texture, g.fence_texture1, g.fence_texture0 = snap.u32(G.RVA["wall"]), snap.u32(G.RVA["fence1"]), snap.u32(G.RVA["fence0"])
        g.screen_scale_bits = snap.u32(G.RVA["scale"])
        # pathUpdateRender keeps the quadrant the previous pass left (corpus g3 recorded 0 before each injected angle); in the app the
        # pass before it ran at the same camera, so the quadrant is the angle's
        g.view_index = view
        assert portlib.dll.sg_gl_set_globals(st.ctx, C.byref(g)) == 0
        info["fence0"] = g.fence_texture0
        info["path_tex"] = {keep[2][0x20 * 0x19 * 9], keep[2][0x21 * 0x19 * 9]}
        portlib.dll.sg_gl_negctl(negctl)
        try:
            out = C.create_string_buffer(1 << 24)
            ln = C.c_size_t(0)
            a = [snap.meta.get(f"entry_a{i}", 0) for i in range(3)]
            code = portlib.dll.sg_gl_render(st.ctx, snap.kind, snap.terrain, a[0], a[1], a[2], out, 1 << 24, C.byref(ln))
        finally:
            portlib.dll.sg_gl_negctl(0)
        assert code == 0, code
        return out.raw[:ln.value].decode().splitlines(), info


def _steps(fans, info):
    """(treads: path texture with the up normal, risers / sides: the retaining-wall texture)"""
    treads = [f for f in fans if f[0] in info["path_tex"] and f[1] and all(abs(nv[1] - 1.0) < 1e-6 and abs(nv[0]) < 1e-6 for nv in f[1])]
    walls = [f for f in fans if f[0] == info["fence0"]]
    return treads, walls


def _need_g3():
    from port.test import glcorpus as G
    if not all((G.CORPUS / "g3" / "snaps" / p).is_dir() for p in PASSES.values()):
        pytest.skip("GL corpus g3 not present (port/corpus/gl/g3)")


def _near(p, t, row_al):
    """is point p over tile t (GL x = tile x * 100 - 2500 .. + 100, z likewise)"""
    return t[0] * 100 - 2500 - 1 <= p[0] <= t[0] * 100 - 2400 + 1 and t[1] * 100 - 2500 - 1 <= p[2] <= t[1] * 100 - 2400 + 1


@need_exe
@pytest.mark.parametrize("axis", ["x", "y"])
@pytest.mark.parametrize("view", [0, 1, 2, 3])
def test_stairs_drawn_four_low_steps_per_level(portlib, view, axis):
    _need_g3()
    off, _ = _render(portlib, view, False, axis)
    on, info = _render(portlib, view, True, axis)
    assert info["axes"][TX if axis == "x" else TY] == (1 if axis == "x" else 2), info["axes"]
    assert off != on, "nothing changed at 0"
    treads, walls = _steps(_fans(on), info)
    # at 1 the tile has its band (path texture, following the slope) and no wall; at 0 no band is left on it
    t = TX if axis == "x" else TY
    band1 = [f for f in _fans(off) if f[0] in info["path_tex"] and f[3] and all(_near(p, t, 0) for p in f[3])]
    assert band1 and all(len({round(p[1], 3) for p in f[3]}) > 1 for f in band1), band1
    assert not [f for f in _fans(off) if f[0] == info["fence0"] and f[3] and all(_near(p, t, 0) for p in f[3])]
    assert not [f for f in _fans(on) if f[0] in info["path_tex"] and f[3] and all(_near(p, t, 0) for p in f[3])
                and len({round(p[1], 3) for p in f[3]}) > 1]
    for t, al in ((TX, 0),) if axis == "x" else ((TY, 2),):
        h0 = info["base"][t]
        mine = [f for f in treads if all(_near(p, t, al) for p in f[3])]
        heights = sorted({round(f[3][0][1] - h0, 3) for f in mine})
        assert heights == TREADS, (t, heights)        # twelve low treads for three levels, on the riser lattice, the top one at
        assert len(mine) == len(TREADS), len(mine)    # the path beyond (base + 45): no partial first / last step, no tread split
        for f in mine:
            assert len({round(p[1], 3) for p in f[3]}) == 1, f[3]  # flat
        # evenly spaced along the axis: every tread is 100 / 12 long (the tile's 100 split by its 12 risers)
        lens = sorted(round(max(p[al] for p in f[3]) - min(p[al] for p in f[3]), 2) for f in mine)
        assert lens[0] > 8.32 and lens[-1] < 8.35, lens
        # the treads go far to near: along the axis, the camera's side last (view 0: camera toward -x and +z)
        cam = (-1 if view in (0, 1) else 1) if al == 0 else (1 if view in (0, 3) else -1)
        mids = [sum(p[al] for p in f[3]) / 4 for f in mine]
        assert all(cam * (b - a) >= -1e-3 for a, b in zip(mids, mids[1:])), (t, view, mids)
        # the risers face the low side (-x / -z: the ramp rises toward +x / +z): twelve of a quarter level each (the low edge and
        # eleven inside), vertical, sent in both windings, when the camera is on the low side; none from the high side (a face turned
        # away is not drawn: with no depth test it covered the treads)
        rs = [f for f in walls if all(_near(p, t, al) for p in f[3]) and len({round(p[al], 3) for p in f[3]}) == 1]
        hs = sorted(round(max(p[1] for p in f[3]) - min(p[1] for p in f[3]), 3) for f in rs)
        assert hs == ([RISE] * 24 if cam < 0 else []), (t, view, hs)
        # the side walls stay within one riser of the ground line (trimmed to the steps)
        ac0 = 2 - al
        side_h = [max(p[1] for p in f[3]) - min(p[1] for p in f[3]) for f in walls
                  if all(_near(p, t, al) for p in f[3]) and len({round(p[ac0], 3) for p in f[3]}) == 1]
        assert side_h and max(side_h) <= RISE + 1e-3, max(side_h)
        # the side walls: only on the camera's side across the axis
        ac = 2 - al
        cam_ac = (1 if view in (0, 3) else -1) if ac == 2 else (-1 if view in (0, 1) else 1)
        centre = (t[1] if ac == 2 else t[0]) * 100 - 2500 + 50
        sides = [f for f in walls if all(_near(p, t, al) for p in f[3]) and len({round(p[ac], 3) for p in f[3]}) == 1]
        assert sides and all((f[3][0][ac] - centre) * cam_ac > 0 for f in sides), (t, view, [f[3][0][ac] - centre for f in sides])
    # without stairs in the sim grids the stream is the same at 0 and 1
    flat_off, _ = _render(portlib, view, False, None)
    flat_on, _ = _render(portlib, view, True, None)
    assert flat_off == flat_on


@need_exe
@pytest.mark.parametrize("view", [0, 2])
def test_stairs_drawing_controls_read_red(portlib, view):
    _need_g3()
    off, _ = _render(portlib, view, False)
    on20, info = _render(portlib, view, True, negctl=20)
    treads, _ = _steps(_fans(on20), info)
    hs = sorted({round(f[3][0][1] - info["base"][TX], 3) for f in treads if all(_near(p, TX, 0) for p in f[3])})
    assert hs != TREADS, hs          # one tread per span: RED of test_stairs_drawn_four_low_steps_per_level
    on23, info = _render(portlib, view, True, negctl=23)
    treads, _ = _steps(_fans(on23), info)
    hs = sorted({round(f[3][0][1] - info["base"][TX], 3) for f in treads if all(_near(p, TX, 0) for p in f[3])})
    assert hs == [15.0, 30.0, 45.0] and hs != TREADS, hs   # P39A2's one step per level: RED of the twelve-tread check
    on21, info = _render(portlib, view, True, negctl=21)
    treads, _ = _steps(_fans(on21), info)
    mine = [f for f in treads if all(_near(p, TX, 0) for p in f[3])]
    mids = [sum(p[0] for p in f[3]) / 4 for f in mine]
    cam = -1 if view in (0, 1) else 1
    assert not all(cam * (b - a) >= -1e-3 for a, b in zip(mids, mids[1:])), mids   # near to far: RED of the order check
    # 22: the faces turned away drawn too: from the high side (views 2 / 3 on TX) risers come back: RED of the riser check
    on22, info = _render(portlib, view, True, negctl=22)
    _t, walls = _steps(_fans(on22), info)
    rs = [f for f in walls if all(_near(p, TX, 0) for p in f[3]) and len({round(p[0], 3) for p in f[3]}) == 1]
    assert len(rs) == 24, len(rs)   # every riser, whatever the camera side
    on, info = _render(portlib, view, True)
    _t, walls0 = _steps(_fans(on), info)
    assert len(walls) > len(walls0), (len(walls), len(walls0))


@need_exe
@pytest.mark.parametrize("axis", ["x", "y"])
@pytest.mark.parametrize("view", [0, 1, 2, 3])
def test_two_level_slope_drawn_as_steps(portlib, view, axis):
    """P39A2b: a 2-level path slope is drawn as steps (drawing only: stairs.axes stays 0, so price and thought are the original's)"""
    _need_g3()
    t, al = (TX, 0) if axis == "x" else (TY, 2)
    off, _ = _render(portlib, view, False, axis, levels=2)
    on, info = _render(portlib, view, True, axis, levels=2)
    assert info["axes"][t] == 0, info["axes"]                 # no sim stairs at 2 levels
    treads, walls = _steps(_fans(on), info)
    mine = [f for f in treads if all(_near(p, t, al) for p in f[3])]
    heights = sorted({round(f[3][0][1] - info["base"][t], 3) for f in mine})
    assert heights == [round(RISE * i, 3) for i in range(1, 9)], heights   # 8 low treads, the top one level with the path beyond
    assert len(mine) == 8, len(mine)
    lens = sorted(round(max(p[al] for p in f[3]) - min(p[al] for p in f[3]), 2) for f in mine)
    assert lens[0] > 12.49 and lens[-1] < 12.51, lens         # 100 / 8, evenly spaced
    assert not [f for f in _fans(on) if f[0] in info["path_tex"] and f[3] and all(_near(p, t, 0) for p in f[3])
                and len({round(p[1], 3) for p in f[3]}) > 1]   # no sloped band left
    # at 1 the original band; control 24 (only the sim's stairs drawn) keeps the band too: RED of the tread check
    band = [f for f in _fans(off) if f[0] in info["path_tex"] and f[3] and all(_near(p, t, 0) for p in f[3])]
    assert band and all(len({round(p[1], 3) for p in f[3]}) > 1 for f in band)
    on24, info24 = _render(portlib, view, True, axis, negctl=24, levels=2)
    t24, _w = _steps(_fans(on24), info24)
    assert not [f for f in t24 if all(_near(p, t, al) for p in f[3]) and len({round(p[1], 3) for p in f[3]}) == 1]   # no flat tread
    assert [f for f in _fans(on24) if f[0] in info24["path_tex"] and f[3] and all(_near(p, t, 0) for p in f[3])
            and len({round(p[1], 3) for p in f[3]}) > 1]
