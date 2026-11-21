"""P39C (re/roadmap/P39_COURSE_FEATURES.md "P39C", log/roadmap/P39/P39C_notes.md): the app option --green-highlight replaces
P1_flag80's blinking cup-tile marker, while the player aims, by an outline of the cup's whole green (greenline.h).

- The tile set and the sides (greenLine.tiles / greenLine.edges) equal a Python reference of the rule on synthetic courses at
  N = 50 and N = 101: green tiles (type 1) 8-connected from the cup tile and its 3 x 3 neighbours, a green with two cups split by
  the nearer cup (ties to the lower hole), a cup on no green gives nothing; the cache follows a tile edit.
- The lines (greenLine.lines) are worldToScreen of each side's ends (the registered body, margin 1 << 24) in the marker's colour,
  and on a sloped green they differ from the flat green's by the terrain height.
- No GameState write: the image (and at N = 101 the side grids) are unchanged by every call.
- A frame run of corpus segments that start with the player aiming (cov_a_champ): option on and off both reproduce the recorded
  sim hashes, end on the same image, and their callee counts differ only by the marker's Sprite_draw calls (fewer with the option)
  and the outline's drawMapLine calls (more): every other drop is unchanged. With the option off nothing of P39C runs (the
  segments gate holds the off path to the recordings).
Negative control: the reference with a 4-connected flood differs from the port on the layout's diagonal tile (the test sees it).
"""
from __future__ import annotations

import ctypes as C
import struct

import pytest

from port.test import lib as L
from port.test.test_terrain import need_exe, new_state, objs
from port.test.test_terrain_gen import _fixed_image

pytestmark = pytest.mark.port

OUT_VA = 0x30000000
KFAR = 1 << 24
WHITE = 0x80007FFF


# ---------------------------------------------------------------------------------------------------- the reference
def ref_outline(types, flags, n, hole, conn8=True):
    last = n - 1
    idx = lambda x, y: x * n + y  # noqa: E731
    green = lambda x, y: 0 <= x <= last and 0 <= y <= last and types[idx(x, y)] == 1  # noqa: E731
    cups = [(x, y, flags[idx(x, y)] & 0x1F) for x in range(n) for y in range(n)
            if flags[idx(x, y)] & 0x80 and flags[idx(x, y)] & 0x1F]
    nb = [(dx, dy) for dx in (-1, 0, 1) for dy in (-1, 0, 1)] if conn8 else [(0, 0), (1, 0), (-1, 0), (0, 1), (0, -1)]
    seen, stack = set(), []
    for cx, cy, h in cups:
        if h == hole:
            for dx in (-1, 0, 1):
                for dy in (-1, 0, 1):
                    if green(cx + dx, cy + dy) and (cx + dx, cy + dy) not in seen:
                        seen.add((cx + dx, cy + dy)); stack.append((cx + dx, cy + dy))
    while stack:
        x, y = stack.pop()
        for dx, dy in nb:
            q = (x + dx, y + dy)
            if green(*q) and q not in seen:
                seen.add(q); stack.append(q)
    mine = [c for c in cups if c[2] == hole]
    rivals = [c for c in cups if c[2] != hole and any((c[0] + dx, c[1] + dy) in seen for dx in (-1, 0, 1) for dy in (-1, 0, 1))]
    d2 = lambda c, x, y: (x - c[0]) ** 2 + (y - c[1]) ** 2  # noqa: E731
    keep = set()
    for (x, y) in seen:
        dm = min(d2(c, x, y) for c in mine)
        if not any(d2(c, x, y) < dm or (d2(c, x, y) == dm and c[2] < hole) for c in rivals):
            keep.add((x, y))
    tiles = sorted(idx(x, y) for x, y in keep)
    edges = []
    for i in tiles:
        x, y = divmod(i, n)
        x0, y0, x1, y1 = x << 10, y << 10, ((x + 1) << 10) - 1, ((y + 1) << 10) - 1
        if (x - 1, y) not in keep: edges.append((x0, y0, x0, y1))
        if (x + 1, y) not in keep: edges.append((x1, y0, x1, y1))
        if (x, y - 1) not in keep: edges.append((x0, y0, x1, y0))
        if (x, y + 1) not in keep: edges.append((x0, y1, x1, y1))
    return tiles, edges


def layout(n):
    """A course of rough (type 2) with: hole 1's green (a block plus one tile touching it only diagonally) and a separate green
    that does not touch it; holes 2 and 3 on one shared green; hole 4's cup on rough; hole 5's cup on rough beside a green."""
    s = n / 50.0
    T = bytearray([2]) * (n * n)
    F = [0] * (n * n)
    def g(x, y): T[x * n + y] = 1
    def cup(x, y, h): F[x * n + y] |= 0x80 | h
    p = lambda v: int(round(v * s))  # noqa: E731
    for x in range(p(9), p(9) + 4):
        for y in range(p(8), p(8) + 4):
            g(x, y)
    g(p(9) + 4, p(8) + 4)                                  # diagonal neighbour of the block's corner
    for x in range(p(20), p(20) + 3):
        for y in range(p(20), p(20) + 2):
            g(x, y)                                         # not connected to hole 1's green
    cup(p(9) + 1, p(8) + 1, 1)
    for x in range(p(30), p(30) + 4):
        for y in range(p(5), p(5) + 10):
            g(x, y)
    cup(p(30) + 1, p(5) + 1, 2)
    cup(p(30) + 2, p(5) + 8, 3)
    cup(p(40), p(40), 4)
    for x in range(p(44), p(44) + 2):
        for y in range(p(3), p(3) + 2):
            g(x, y)
    cup(p(44) - 1, p(3) - 1, 5)                            # rough tile, diagonal to the green's corner
    return bytes(T), F


# ---------------------------------------------------------------------------------------------------- helpers
def _state(portlib, n):
    st = new_state(portlib).__enter__()
    img = bytearray(_fixed_image(portlib))
    o = objs(portlib)
    def put(name, data, at=0):
        off = o[name][0]
        img[off + at:off + at + len(data)] = data
    put("g_date", struct.pack("<i", 0x4B0C))
    put("g_zoomScale", struct.pack("<i", 4))
    put("g_cameraRot", struct.pack("<i", 0))
    put("g_screenWidth", struct.pack("<i", 800))
    put("g_screenHeight", struct.pack("<i", 600))
    put("f_w_4c2e00", struct.pack("<i", 5))   # the height step worldToScreen scales by (5 in every recorded start, cov_a_champ/00)
    st.load_image(bytes(img))
    if n != 50:
        st.world_resize(n)
    T, F = layout(n)
    st.mem_write(st.grid_va("g_tileType"), T)
    st.mem_write(st.grid_va("g_tileFlags"), struct.pack(f"<{n * n}H", *F))
    st.mem_write(st.grid_va("g_cornerLevel"), bytes([4]) * ((n + 1) ** 2))
    return st, T, F


def _grids(st, n):
    return (st.mem_read(st.grid_va("g_tileType"), n * n) + st.mem_read(st.grid_va("g_tileFlags"), 2 * n * n)
            + st.mem_read(st.grid_va("g_cornerLevel"), (n + 1) ** 2))


CAP = 4096
_BUF = {}


def _buf(st):
    """the scratch out-buffer at OUT_VA, mapped once per state (unmap_all would drop the exe and side-storage mappings too)"""
    if id(st) not in _BUF:
        _BUF[id(st)] = st.map(OUT_VA, (C.c_int32 * (7 * CAP))())
    return _BUF[id(st)]


def _tiles(st, hole):
    b = _buf(st)
    k = st.call("greenLine.tiles", [hole, OUT_VA, CAP]).eax
    return list(b[:k])


def _edges(st, hole):
    b = _buf(st)
    k = st.call("greenLine.edges", [hole, OUT_VA, CAP]).eax
    return [tuple(b[4 * i:4 * i + 4]) for i in range(k)]


def _lines(st):
    b = _buf(st)
    k = st.call("greenLine.lines", [OUT_VA, CAP]).eax
    return [tuple(v & 0xFFFFFFFF for v in b[7 * i:7 * i + 7]) for i in range(k)]


def _w2s(st, wx, wy):
    b = _buf(st)
    b[0] = b[1] = 0
    st.call("worldToScreen", [wx & 0xFFFFFFFF, wy & 0xFFFFFFFF, OUT_VA, OUT_VA + 4, KFAR])
    return (b[0], b[1])


def _s32(v):
    return v - (1 << 32) if v & 0x80000000 else v


def _aim(portlib, st, hole, cam):
    o = objs(portlib)
    st.write(o["g_panelListMode"][0], struct.pack("<i", 3))
    st.write(o["g_playerGolfer"][0], struct.pack("<i", 0))
    st.write(o["g_golfers"][0] + 0x21, struct.pack("<b", hole))
    st.write(o["gCameraTileX"][0], struct.pack("<i", cam[0]))
    st.write(o["gCameraTileY"][0], struct.pack("<i", cam[1]))


# ---------------------------------------------------------------------------------------------------- tests
@need_exe
@pytest.mark.parametrize("n", [50, 101])
def test_tiles_and_edges_equal_reference(portlib, n):
    st, T, F = _state(portlib, n)
    try:
        img0, g0 = st.image(), _grids(st, n)
        for hole in range(1, 7):
            rt, re_ = ref_outline(T, F, n, hole)
            assert _tiles(st, hole) == rt, f"hole {hole} tiles"
            assert _edges(st, hole) == re_, f"hole {hole} edges"
        # hole 1 includes the diagonal tile, not the separate green; holes 2 + 3 partition the shared green; 4 and 6 are empty
        t1 = set(_tiles(st, 1))
        s = n / 50.0
        p = lambda v: int(round(v * s))  # noqa: E731
        assert (p(9) + 4) * n + p(8) + 4 in t1 and p(20) * n + p(20) not in t1 and len(t1) == 17
        t2, t3 = set(_tiles(st, 2)), set(_tiles(st, 3))
        assert not (t2 & t3) and len(t2 | t3) == 40 and t2 and t3
        assert _tiles(st, 4) == [] and _tiles(st, 6) == [] and len(_tiles(st, 5)) == 4
        assert st.image() == img0 and _grids(st, n) == g0, "greenLine wrote the GameState"
        # the cache follows a tile edit: one more green tile beside hole 1's block
        e1 = _edges(st, 1)
        T2 = bytearray(T)
        T2[(p(9) - 1) * n + p(8)] = 1
        st.mem_write(st.grid_va("g_tileType"), bytes(T2))
        e1b = _edges(st, 1)
        assert e1b != e1 and e1b == ref_outline(bytes(T2), F, n, 1)[1]
    finally:
        _BUF.pop(id(st), None)
        st.close()


@need_exe
def test_negative_control_four_connected_reference_differs(portlib):
    st, T, F = _state(portlib, 50)
    try:
        assert ref_outline(T, F, 50, 1, conn8=False)[0] != _tiles(st, 1), "the 4-connected reference matched: test is blind"
    finally:
        _BUF.pop(id(st), None)
        st.close()


@need_exe
def test_lines_follow_world_to_screen_and_slope(portlib):
    st, T, F = _state(portlib, 50)
    prev_square = st.call("greenLine.square", [1]).eax   # P39C2_2: the tile sides / whole tiles
    try:
        _aim(portlib, st, 1, (11, 10))
        img0 = st.image()
        edges = _edges(st, 1)
        flat = _lines(st)
        assert len(flat) == len(edges) > 0
        for (x0, y0, x1, y1), ln in zip(edges, flat):
            a, b = _w2s(st, x0, y0), _w2s(st, x1, y1)
            assert tuple(_s32(v) for v in ln[:4]) == (a[0], a[1], b[0], b[1])
            assert ln[4:] == (WHITE, 3, 7)
        # a slope under the green: the vertex levels rise along y; the lines are the new projections and lie higher on screen
        lv = bytearray([4]) * (51 * 51)
        for x in range(51):
            for y in range(51):
                lv[x * 51 + y] = min(15, 3 + y // 2)
        st.mem_write(st.grid_va("g_cornerLevel"), bytes(lv))
        img1 = st.image()
        sloped = _lines(st)
        assert len(sloped) == len(flat)
        moved = 0
        for (x0, y0, x1, y1), ln, lf in zip(edges, sloped, flat):
            a, b = _w2s(st, x0, y0), _w2s(st, x1, y1)
            assert tuple(_s32(v) for v in ln[:4]) == (a[0], a[1], b[0], b[1])
            moved += _s32(ln[1]) != _s32(lf[1])
        assert moved == len(flat)
        assert st.image() == img1 and img0 != img1
        # not aiming (g_panelListMode != 3) or no player golfer: no lines
        o = objs(portlib)
        st.write(o["g_panelListMode"][0], struct.pack("<i", 2))
        assert _lines(st) == []
        st.write(o["g_panelListMode"][0], struct.pack("<i", 3))
        st.write(o["g_playerGolfer"][0], struct.pack("<i", -1))
        assert _lines(st) == []
    finally:
        st.call("greenLine.square", [prev_square])
        _BUF.pop(id(st), None)
        st.close()


def _fill(st):
    b = _buf(st)
    cap = 7 * CAP // 8
    k = st.call("greenLine.fill", [OUT_VA, cap]).eax
    return [(tuple(b[8 * i:8 * i + 4]), tuple(b[8 * i + 4:8 * i + 8])) for i in range(min(k, cap))], k


@need_exe
def test_fill_quads_are_the_tiles_corners(portlib):
    """P39C2: greenLine.fill gives one quad per tile of the outline's set on screen, its corners (x, y) in 1 / 16 px around the tile
    (x, y), (x + 1, y), (x + 1, y + 1), (x, y + 1) within a pixel of worldToScreen's; neighbouring tiles share their corners exactly
    (no seam), a slope moves them, a camera far away leaves none; no GameState write."""
    st, T, F = _state(portlib, 50)
    prev_square = st.call("greenLine.square", [1]).eax   # P39C2_2: the tile sides / whole tiles
    try:
        _aim(portlib, st, 1, (11, 10))
        img0, g0 = st.image(), _grids(st, 50)
        tiles = _tiles(st, 1)
        quads, k = _fill(st)
        assert k == len(quads) == len(tiles) == 17
        by_tile = dict(zip(tiles, quads))
        for t, (xs, ys) in by_tile.items():
            x, y = t // 50, t % 50
            for (wx, wy), qx, qy in zip([(x, y), (x + 1, y), (x + 1, y + 1), (x, y + 1)], xs, ys):
                a = _w2s(st, wx << 10, wy << 10)
                assert abs(qx / 16.0 - _s32(a[0])) <= 1.0 and abs(qy / 16.0 - _s32(a[1])) <= 1.0, (t, wx, wy, qx, qy, a)
        shared = 0
        for t, (xs, ys) in by_tile.items():
            if t + 50 in by_tile:   # (x + 1, y): its corners 0, 3 are this tile's 1, 2
                xs2, ys2 = by_tile[t + 50]
                assert (xs2[0], ys2[0], xs2[3], ys2[3]) == (xs[1], ys[1], xs[2], ys[2])
                shared += 1
            if t + 1 in by_tile:    # (x, y + 1): its corners 0, 1 are this tile's 3, 2
                xs2, ys2 = by_tile[t + 1]
                assert (xs2[0], ys2[0], xs2[1], ys2[1]) == (xs[3], ys[3], xs[2], ys[2])
                shared += 1
        assert shared > 0
        assert st.image() == img0 and _grids(st, 50) == g0, "greenLine.fill wrote the GameState"
        # a slope: the same tiles, the corners higher on screen where the levels rose
        lv = bytearray([4]) * (51 * 51)
        for x in range(51):
            for y in range(51):
                lv[x * 51 + y] = min(15, 3 + y // 2)
        st.mem_write(st.grid_va("g_cornerLevel"), bytes(lv))
        sq, k2 = _fill(st)
        assert k2 == len(quads)
        assert sum(a[1] != b[1] for a, b in zip(sq, quads)) == len(quads)
        # the camera far from the green: every quad off the view
        st.write(objs(portlib)["gCameraTileX"][0], struct.pack("<i", 45))
        st.write(objs(portlib)["gCameraTileY"][0], struct.pack("<i", 45))
        assert _fill(st)[1] == 0
        # not aiming: none
        _aim(portlib, st, 1, (11, 10))
        st.write(objs(portlib)["g_panelListMode"][0], struct.pack("<i", 2))
        assert _fill(st)[1] == 0
    finally:
        st.call("greenLine.square", [prev_square])
        _BUF.pop(id(st), None)
        st.close()


@need_exe
def test_option_switch(portlib):
    with new_state(portlib) as st:
        prev = st.call("greenLine.option", [1]).eax
        assert prev == 0, "the library default is off (the app sets it)"
        assert st.call("greenLine.option", [0]).eax == 1
        assert st.call("greenLine.option", [0]).eax == 0


# ---------------------------------------------------------------------------------------------------- P39C2_2: the putting surface
# The reference: blendFaceTextures' piece per face (terrain_state.cpp blend_face_textures, edge_kind, flag_code, id_class) and the
# port's piece masks (greenLine.pieceMask; their source is checked against the texture files below), composed per tile, the strips
# rebuilt from those masks.
def _flag_piece(a, b, c):
    r = (4 if a == 1 else 0x28 if a == 2 else 0) + (1 if b == 1 else 10 if b == 2 else 0) + (2 if c == 1 else 0x14 if c == 2 else 0)
    table = {0: 0, 1: 0, 2: 0, 10: 0, 0xC: 0, 0x14: 0, 0x15: 0, 3: 2, 0x2B: 2, 4: 4, 0xE: 4, 0x18: 4, 0x22: 4, 5: 3, 7: 3, 0x19: 3,
             6: 1, 0x10: 1, 0x28: 8, 0x29: 8, 0x2A: 8, 0x3D: 8, 0x32: 7, 0x34: 7, 0x46: 7, 0x3C: 5, 0x1E: 6}
    return table.get(r, 0)


def _collars(portlib, st):
    img = st.image()
    off = objs(portlib)["g_tileTypeAttrs"][0]
    course = struct.unpack_from("<b", img, objs(portlib)["g_courseType"][0])[0]
    out = []
    for t in range(23):
        walk, col = struct.unpack_from("<bxxxb", img, off + 0x30 * t + 0x22)
        out.append(4 if course == 1 and walk > 0 and t not in (5, 7) else col)
    return out


def _pieces(st):
    b = _buf(st)
    masks = []
    for p in range(5):
        assert st.call("greenLine.pieceMask", [p, OUT_VA]).eax == 64
        masks.append([(b[2 * r] & 0xFFFFFFFF) | ((b[2 * r + 1] & 0xFFFFFFFF) << 32) for r in range(64)])
    return masks


def ref_tile_mask(types, n, x, y, collar, masks, use_diag=True):
    own = types[x * n + y]
    col = lambda t: collar[t] if 0 <= t < 23 else 0x100 + t  # noqa: E731

    def kind(dx, dy):
        nx, ny = x + dx, y + dy
        if not (0 <= nx < n and 0 <= ny < n):
            return 0
        t = types[nx * n + ny]
        return 1 if col(t) != col(own) else 2 if t != own else 0
    rows = []
    for r in range(64):
        qy, dy = (1, 63 - r) if r >= 32 else (0, r)
        v = 0
        for c in range(64):
            qx, dx = (1, 63 - c) if c >= 32 else (0, c)
            sx, sy = (1 if qx else -1), (1 if qy else -1)
            ky, kx, kd = kind(0, sy), kind(sx, 0), kind(sx, sy) if use_diag else 0
            p = _flag_piece(ky, kx, kd) if dy <= dx else _flag_piece(kx, ky, kd)
            if p > 4 or (masks[p][r] >> c) & 1:
                v |= 1 << c
        rows.append(v)
    return rows


def ref_strips(x, y, rows):
    out, r0 = [], 0
    for r in range(1, 65):
        if r < 64 and rows[r] == rows[r0]:
            continue
        row, c = rows[r0], 0
        while c < 64:
            if not (row >> c) & 1:
                c += 1
                continue
            e = c
            while e < 64 and (row >> e) & 1:
                e += 1
            out.append(((x << 10) + 16 * c, (y << 10) + 16 * r0, (x << 10) + 16 * e, (y << 10) + 16 * r))
            c = e
        r0 = r
    return out


def _tile_mask(st, x, y):
    b = _buf(st)
    assert st.call("greenLine.tileMask", [x, y, OUT_VA]).eax == 64
    return [(b[2 * r] & 0xFFFFFFFF) | ((b[2 * r + 1] & 0xFFFFFFFF) << 32) for r in range(64)]


def _quad4(st, name, hole):
    b = _buf(st)
    k = st.call(name, [hole, OUT_VA, CAP]).eax
    assert k <= CAP
    return [tuple(b[4 * i:4 * i + 4]) for i in range(k)]


@need_exe
def test_piece_masks_come_from_the_green_textures(portlib):
    """kGreenPieceMask = the bright texels of the Terrain's own PuttingGreenA0001..0005.bmp (Parkland, piece p = file p + 1): summed
    RGB above the midpoint between piece 0's median and piece 3's 5th percentile, then each texel the majority of its 8 images under
    the square's rotations and mirrors (log/roadmap/P39/P39C2_work/piece_table.py, the generator)"""
    from port.test import segments as S
    tex = S.INSTALL / "Data" / "Textures" / "Parkland"
    if not (tex / "PuttingGreenA0001.bmp").exists():
        pytest.skip("install textures not present")
    import numpy as np
    from PIL import Image
    lum = [np.asarray(Image.open(tex / f"PuttingGreenA000{k}.bmp").convert("RGB")).astype(int).sum(2) for k in range(1, 6)]
    t = (np.median(lum[0]) + np.percentile(lum[3], 5)) / 2
    with new_state(portlib) as st:
        got = _pieces(st)
        _BUF.pop(id(st), None)
    for p in range(5):
        a = lum[p] > t
        v = [a, a[:, ::-1], a[::-1, :], a[::-1, ::-1], a.T, a.T[:, ::-1], a.T[::-1, :], a.T[::-1, ::-1]]
        want = sum(x.astype(int) for x in v) >= 5
        for r in range(64):
            assert got[p][r] == sum(1 << c for c in range(64) if want[r, c]), (p, r)


@need_exe
def test_tile_masks_and_strips_equal_reference(portlib):
    st, T, F = _state(portlib, 50)
    try:
        img0 = st.image()
        collar, masks = _collars(portlib, st), _pieces(st)
        assert masks[0] == [(1 << 64) - 1] * 64 and masks[3][0] == 0 and masks[3][32] != (1 << 64) - 1
        for hole in (1, 2, 3, 5):
            tiles = _tiles(st, hole)
            want = []
            for t in tiles:
                x, y = divmod(t, 50)
                rows = ref_tile_mask(T, 50, x, y, collar, masks)
                assert _tile_mask(st, x, y) == rows, f"hole {hole} tile {(x, y)}"
                want += ref_strips(x, y, rows)
            assert _quad4(st, "greenLine.strips", hole) == want, f"hole {hole} strips"
        # the shapes the layout gives: a straight side keeps the fringe band (piece 1), a block's outer corner is rounded (piece 3),
        # the split between holes 2 and 3 on one green has no band (same type both sides)
        x, y = 9, 8                                   # hole 1's block corner tile
        m = _tile_mask(st, x, y)
        assert not (m[0] & 1) and not (m[3] & (1 << 3)) and (m[32] >> 32) & 1 and not (m[32] & 1)
        t2 = set(_tiles(st, 2))
        for t in t2:                                  # tiles of hole 2 whose y + 1 neighbour is hole 3's: lit to their last row
            if t + 1 in set(_tiles(st, 3)):
                assert (_tile_mask(st, *divmod(t, 50))[63] >> 32) & 1
        assert st.image() == img0, "the shape wrote the GameState"
    finally:
        _BUF.pop(id(st), None)
        st.close()


@need_exe
def test_negative_control_reference_without_the_diagonal_differs(portlib):
    st, T, F = _state(portlib, 50)
    try:
        collar, masks = _collars(portlib, st), _pieces(st)
        differ = sum(_tile_mask(st, *divmod(t, 50)) != ref_tile_mask(T, 50, *divmod(t, 50), collar, masks, use_diag=False)
                     for t in _tiles(st, 1))
        assert differ > 0, "the reference without the corner's diagonal tile matched: the test is blind to it"
    finally:
        _BUF.pop(id(st), None)
        st.close()


@need_exe
def test_shape_is_the_lit_boundary(portlib):
    """greenLine.shape: closed loops (every end is one segment's start and another's end), every vertex on the texel lattice where
    lit and unlit texels meet, and the loops' area (shoelace, the surface on the left) within 1 % of the lit texels' area"""
    st, T, F = _state(portlib, 50)
    try:
        for hole in (1, 2, 3, 5):
            tiles = set(_tiles(st, hole))
            lit = {}
            for t in tiles:
                x, y = divmod(t, 50)
                lit[(x, y)] = _tile_mask(st, x, y)
            def L_(gx, gy):
                x, y = gx >> 6, gy >> 6
                rows = lit.get((x, y))
                return bool(rows and (rows[gy & 63] >> (gx & 63)) & 1)
            segs = _quad4(st, "greenLine.shape", hole)
            assert segs
            starts = sorted((a, b) for a, b, _, _ in segs)
            ends = sorted((c, d) for _, _, c, d in segs)
            assert starts == ends, f"hole {hole}: the shape is not closed"
            area = 0
            for x0, y0, x1, y1 in segs:
                assert x0 % 16 == 0 and y0 % 16 == 0
                gx, gy = x0 // 16, y0 // 16
                around = {L_(gx - 1, gy - 1), L_(gx, gy - 1), L_(gx - 1, gy), L_(gx, gy)}
                assert around == {True, False}, f"hole {hole}: vertex {(x0, y0)} not on the lit boundary"
                area += x0 * y1 - x1 * y0
            lit_area = sum(bin(v).count("1") for rows in lit.values() for v in rows) * 256
            assert abs(area / 2 - lit_area) <= 0.03 * lit_area, (hole, area / 2, lit_area)
    finally:
        _BUF.pop(id(st), None)
        st.close()


def _shape_point(tiles, wx, wy, n=50):
    inn = lambda x, y: 0 <= x < n and 0 <= y < n and x * n + y in tiles  # noqa: E731
    tx, ty = wx >> 10, wy >> 10
    if inn(tx, ty):
        return wx, wy
    ex, ey = wx & 1023 == 0, wy & 1023 == 0
    if ex and inn(tx - 1, ty):
        return wx - 1, wy
    if ey and inn(tx, ty - 1):
        return wx, wy - 1
    if ex and ey and inn(tx - 1, ty - 1):
        return wx - 1, wy - 1
    return wx, wy


@need_exe
def test_shape_lines_and_strip_quads_follow_world_to_screen(portlib):
    st, T, F = _state(portlib, 50)
    try:
        _aim(portlib, st, 1, (11, 10))
        tiles = set(_tiles(st, 1))
        segs = _quad4(st, "greenLine.shape", 1)
        strips = _quad4(st, "greenLine.strips", 1)
        lines = _lines(st)
        assert len(lines) == len(segs) > 0
        for (x0, y0, x1, y1), ln in zip(segs, lines):
            a = _w2s(st, *_shape_point(tiles, x0, y0))
            b = _w2s(st, *_shape_point(tiles, x1, y1))
            assert tuple(_s32(v) for v in ln[:4]) == (a[0], a[1], b[0], b[1])
        quads, k = _fill(st)
        assert k == len(quads) == len(strips)
        for (x0, y0, x1, y1), (xs, ys) in zip(strips, quads):
            for (wx, wy), qx, qy in zip([(x0, y0), (x1, y0), (x1, y1), (x0, y1)], xs, ys):
                a = _w2s(st, *_shape_point(tiles, wx, wy))
                assert abs(qx / 16.0 - a[0]) <= 3.0 and abs(qy / 16.0 - a[1]) <= 3.0   # the integer one truncates (bridge.cpp: about 3)
        # a slope under the green moves every line end and every quad
        lv = bytearray([4]) * (51 * 51)
        for x in range(51):
            for y in range(51):
                lv[x * 51 + y] = min(15, 3 + y // 2)
        st.mem_write(st.grid_va("g_cornerLevel"), bytes(lv))
        assert _quad4(st, "greenLine.shape", 1) == segs, "the shape is in world units: the slope does not change it"
        sl = _lines(st)
        assert sum(_s32(a[1]) != _s32(b[1]) for a, b in zip(sl, lines)) == len(lines)
        sq, k2 = _fill(st)
        assert k2 == k and sum(a[1] != b[1] for a, b in zip(sq, quads)) == k
        # the square switch brings the tile sides back (P39C)
        prev = st.call("greenLine.square", [1]).eax
        try:
            assert _quad4(st, "greenLine.shape", 1) == [] and len(_lines(st)) == len(_edges(st, 1))
        finally:
            st.call("greenLine.square", [prev])
    finally:
        _BUF.pop(id(st), None)
        st.close()


# ---------------------------------------------------------------------------------------------------- the frame run
def _seg_ok():
    from port.test import segments as S
    return S.SEG_DIR.exists() and S.INSTALL.is_dir()


@need_exe
@pytest.mark.skipif(not _seg_ok(), reason="segment corpus or install not present")
@pytest.mark.parametrize("name", ["cov_a_champ/00", "cov_a_champ/01"])
def test_frame_run_option_swaps_marker_for_outline_only(portlib, name):
    from port.test import integrator as I
    from port.test import segments as S
    sc, k = name.split("/")
    seg = S.load_segment(S.SEG_DIR / sc / k)
    f = portlib.dll.sg_tick_callee_counts
    f.argtypes = [C.c_char_p, C.c_int32, C.c_int32]

    def run(on):
        with new_state(portlib) as st0:
            st0.call("greenLine.option", [1 if on else 0])
        f(C.create_string_buffer(16), 16, 1)
        end = {}
        res = I.run_from(portlib, seg.name, seg, seg.trace, seg.records, None,
                         after=lambda st, r: end.__setitem__("img", st.image()))
        buf = C.create_string_buffer(1 << 20)
        f(buf, 1 << 20, 0)
        counts = {}
        for line in buf.value.decode().splitlines():
            if "=" in line:
                a, v = line.rsplit("=", 1)
                counts[a] = int(v)
        return res, end.get("img"), counts

    try:
        r_off, img_off, c_off = run(False)
        r_on, img_on, c_on = run(True)
    finally:
        with new_state(portlib) as st0:
            st0.call("greenLine.option", [0])
    assert r_off.status == "PASS", (r_off.status, r_off.detail)
    assert r_on.status == "PASS", (r_on.status, r_on.detail)
    assert img_on == img_off, "the option changed the GameState"
    keys = set(c_off) | set(c_on)
    diff = {k_: (c_off.get(k_, 0), c_on.get(k_, 0)) for k_ in keys if c_off.get(k_, 0) != c_on.get(k_, 0)}
    print(name, "count differences (off, on):", diff, "frames", r_on.frames_run)
    assert set(diff) <= {"Sprite_draw", "drawMapLine"}, diff
    assert "Sprite_draw" in diff and diff["Sprite_draw"][1] < diff["Sprite_draw"][0], diff
    assert "drawMapLine" in diff and diff["drawMapLine"][1] > diff["drawMapLine"][0], diff
