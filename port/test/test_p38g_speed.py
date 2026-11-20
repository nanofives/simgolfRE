"""P38G (log/roadmap/P38/P38G_notes.md): the speed fixes at N = 152 give the same results as before them, and the strip pass at N.

The fixes have a development switch, sg_p38g_off (portsim_c.h; SG_P38G_OFF=1 at start), that runs the code as it was before
them: registry_find's linear search, TkBase::call's locked cache and string-keyed counts, Memory::at's one-entry cache,
rebuildHeightfield without the heightBlend memo, LiveTerrain's whole-map sync passes (render only: judged in the app,
P38G_notes.md). pathStep's hoisted loop reads have no switch: the segments (golfer walks at 50) and test_p38b_world.py's walk at 152
cover them; here its results are compared across the lookups' switch.

Each test runs the same calls from the same state with the switch at 1 and at 0 and compares the GameState and every side grid
byte for byte, at 50 (a course of random tile types, test_p38fe_buyland's) and at 152 (the flat course with the same random types
written over its land)."""
from __future__ import annotations

import ctypes as C
import random
import struct

import pytest

from port.test import lib as L
from port.test import test_commands as TC
from port.test import test_p38fe_buyland as FE

pytestmark = [pytest.mark.port,
              pytest.mark.skipif(not L.exe_path().exists(), reason="the user's golf_clean.exe is not present")]

# (grid, entries per tile, bytes per entry); the vertex grid has (N + 1)^2 entries
GRIDS = [("g_tileType", 1, 1), ("g_tileFlags", 1, 2), ("g_tileLevel", 1, 1), ("g_cornerCache", 8, 1), ("g_tileWallMask", 1, 1),
         ("g_pathGrid", 1, 1), ("g_tileScreenX", 1, 2), ("g_tileScreenY", 1, 2), ("g_tileVariant", 1, 1), ("g_tilePathCost", 1, 1)]


def _off(portlib, v: int) -> int:
    f = portlib.dll.sg_p38g_off
    f.restype, f.argtypes = C.c_int32, [C.c_int32]
    return f(v)


def _snap(st, n: int) -> bytes:
    out = [st.image()]
    if n != 50:
        for g, per, elem in GRIDS:
            out.append(st.mem_read(st.grid_va(g), n * n * per * elem))
        out.append(st.mem_read(st.grid_va("g_cornerLevel"), (n + 1) * (n + 1)))
    return b"".join(out)


def _world(portlib, st, n: int):
    if st.lib.has("audio_exe.stubRequests"):
        st.call("audio_exe.stubRequests", [1])
    st.set_data_base(TC.DATA_VA)
    img = FE._course(portlib, 50)
    st.load_image(img)
    if n == 50:
        return
    st.world_resize(n)
    st.call("generateFlatCourse", [3])
    # the 50 course's tile types tiled over the land (rows / columns 3 .. n - 4), so slopes, water and walls occur
    o = TC.objs(portlib)
    t50 = img[o["g_tileType"][0]:o["g_tileType"][0] + 2500]
    tva = st.grid_va("g_tileType")
    cur = bytearray(st.mem_read(tva, n * n))
    for x in range(3, n - 3):
        for y in range(3, n - 3):
            v = t50[(x % 50) * 50 + (y % 50)]
            if v != 0x14:
                cur[x * n + y] = v
    st.mem_write(tva, bytes(cur))


def _both(portlib, n: int, body) -> list:
    res = []
    for off in (1, 0):
        prev = _off(portlib, off)
        try:
            with portlib.state() as st:
                _world(portlib, st, n)
                r = body(st)
                res.append((r, _snap(st, n)))
        finally:
            _off(portlib, prev)
    return res


@pytest.mark.parametrize("n", [50, 152])
def test_rebuild_heightfield_same_with_and_without_the_memo(portlib, n):
    (r1, s1), (r0, s0) = _both(portlib, n, lambda st: [st.call("rebuildHeightfield", []).eax for _ in range(2)])
    assert r1 == r0
    assert s1 == s0


@pytest.mark.parametrize("n", [50, 152])
def test_tile_queries_same_with_and_without_the_lookups(portlib, n):
    """The registry index, the per-thread callee cache and Memory::at's recent mappings: a sweep of the whole-world callees
    that go through call_peer / the side grids (worldToScreen's height arm calls cornerHeights through call_peer)."""
    rng = random.Random(3)
    pts = [(rng.randrange(0, n << 10), rng.randrange(0, n << 10)) for _ in range(300)]

    def body(st):
        st.call("rebuildHeightfield", [])
        out = []
        for x, y in pts:
            out.append(st.call("heightAt", [x, y]).eax if st.lib.has("heightAt") else 0)
            out.append(st.call("cornerHeights", [x >> 10, y >> 10, 5, 1]).eax)
            out.append(st.call("tileBlocked", [x >> 10, y >> 10]).eax)
        for x in range(0, n, 7):
            for y in range(0, n, 5):
                out.append(st.call("heightBlend", [x, y]).eax)
        return out
    (r1, s1), (r0, s0) = _both(portlib, n, body)
    assert r1 == r0
    assert s1 == s0


@pytest.mark.parametrize("n", [50, 152])
def test_path_steps_same_with_and_without(portlib, n):
    rng = random.Random(11)
    walks = [((rng.randrange(1, n - 1) << 10) + 0x200, (rng.randrange(1, n - 1) << 10) + 0x200,
              (rng.randrange(1, n - 1) << 10) + 0x200, (rng.randrange(1, n - 1) << 10) + 0x200, rng.randrange(0, 8)) for _ in range(40)]

    def body(st):
        st.call("rebuildHeightfield", [])
        return [st.call("pathStep", list(w)).eax for w in walks]
    (r1, s1), (r0, s0) = _both(portlib, n, body)
    assert r1 == r0
    assert s1 == s0


# ---- the strip pass at N (terrain_render.cpp begin_pass): the draw list's size is read after the context's tile array
def _strip_pass(portlib, n: int, list_size: int, junk_at_50_offset: bool) -> int:
    """sg_gl_render's stripRender (entry 1, direction 3) over a Terrain object of N x N hidden tiles; returns its code."""
    with portlib.state() as st:
        st.set_data_base(TC.DATA_VA)
        if n != 50:
            st.world_resize(n)
        tiles = 0x3A4
        size = tiles + n * n * 0x248 + 0x0C
        buf = (C.c_uint8 * size)()
        va = 0x40000000
        st.map(va, buf)
        struct.pack_into("<iiiii", buf, 0x14, n, n, 0, 800, 600)   # width, height, zoom 0, screen 800 x 600
        for y in range(n):
            for x in range(n):
                t = tiles + (x + y * n) * 0x248
                struct.pack_into("<i", buf, t + 0x24, 0x14)        # hidden: nothing is drawn
                struct.pack_into("<ii", buf, t + 0x2C, x, y)
        struct.pack_into("<i", buf, tiles + n * n * 0x248 + 8, list_size)   # the draw list's size at N
        if junk_at_50_offset:
            struct.pack_into("<i", buf, 0x164ACC, 1)              # where the size sits at 50: tile 2,500's data at 152
        pivot = va + tiles + (n // 2 + (n // 2) * n) * 0x248
        cap = 1 << 22
        out = C.create_string_buffer(cap)
        ln = C.c_size_t(0)
        f = portlib.dll.sg_gl_render
        f.restype = C.c_int
        f.argtypes = [C.c_void_p, C.c_int32, C.c_uint32, C.c_uint32, C.c_uint32, C.c_uint32, C.c_char_p, C.c_size_t,
                      C.POINTER(C.c_size_t)]
        return f(st.ctx, 1, va, pivot, 3, 0, out, cap, C.byref(ln))


def test_strip_pass_reads_the_draw_list_at_n(portlib):
    """At 152 the strip pass runs over an object whose 50-layout draw-list word holds tile data (before P38G it threw there, so the
    app dropped every strip pass: edge scrolling showed a new picture about 4 times a second); the guard still reads the size at N."""
    assert _strip_pass(portlib, 152, 0, junk_at_50_offset=True) == 0
    assert _strip_pass(portlib, 152, 1, junk_at_50_offset=False) != 0   # a list left non-empty: the original's unmodelled case
    assert _strip_pass(portlib, 50, 0, junk_at_50_offset=False) == 0
    assert _strip_pass(portlib, 50, 1, junk_at_50_offset=False) != 0
