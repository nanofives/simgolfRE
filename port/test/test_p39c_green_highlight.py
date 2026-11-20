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
        _BUF.pop(id(st), None)
        st.close()


@need_exe
def test_option_switch(portlib):
    with new_state(portlib) as st:
        prev = st.call("greenLine.option", [1]).eax
        assert prev == 0, "the library default is off (the app sets it)"
        assert st.call("greenLine.option", [0]).eax == 1
        assert st.call("greenLine.option", [0]).eax == 0


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
