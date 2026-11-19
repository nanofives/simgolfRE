"""P38B: the first world larger than 50 (re/roadmap/P38_LARGE_MAPS.md phase B; log/roadmap/P38/P38B_notes.md).

Headless, through the C ABI: world_resize(152) maps the side storage, generateFlatCourse makes the flat course, a hole is built
at the far corner after buying that lot (buyLotTiles), and a golfer walks from the clubhouse to it with pathStep (the side
path ring). N = 50 round trips back to the GameState's grids. The control generateFlatCourse 1 (owned square not scaled)
reads RED on the owned share."""
from __future__ import annotations

import struct

import pytest

from port.test.test_terrain import need_exe, new_state, objs
from port.test.test_terrain_gen import _fixed_image

pytestmark = pytest.mark.port

N = 152
COURSE = 3


def _grid(st, name, n_entries, elem=1):
    return st.mem_read(st.grid_va(name), n_entries * elem)


def _flat(portlib, n=N, acres=40):
    st = new_state(portlib).__enter__()
    st.load_image(bytes(_fixed_image(portlib, n=COURSE, acres=acres)))
    st.world_resize(n)
    eax = st.call("generateFlatCourse", [COURSE]).eax
    return st, eax


def _owned(types, n):
    return [(x, y) for x in range(n) for y in range(n) if types[x * n + y] != 0x14]


@need_exe
def test_resize_maps_side_storage_and_back(portlib):
    with new_state(portlib) as st:
        base = st.grid_va("g_tileType")
        assert st.world_n() == 50
        st.world_resize(N)
        assert st.world_n() == N
        va = {g: st.grid_va(g) for g in ("g_tileType", "g_tileFlags", "g_cornerLevel", "g_cornerCache", "g_tileShadeA")}
        assert va["g_tileType"] != base and all(va.values())
        # every grid's N-sized block is mapped and zero, and writable
        assert _grid(st, "g_tileType", N * N) == bytes(N * N)
        assert len(_grid(st, "g_tileFlags", N * N, 2)) == 2 * N * N
        assert len(_grid(st, "g_cornerLevel", (N + 1) ** 2)) == (N + 1) ** 2
        assert len(_grid(st, "g_cornerCache", 8 * N * N)) == 8 * N * N
        st.mem_write(va["g_tileType"] + N * N - 1, b"\x07")
        # grids do not overlap: the last byte of g_tileType is not seen in its neighbours
        assert _grid(st, "g_tileType", N * N)[-1] == 7
        st.world_resize(50)
        assert st.world_n() == 50 and st.grid_va("g_tileType") == base
        with pytest.raises(Exception):
            st.world_resize(151)   # (151 - 2) % 3 != 0


@need_exe
def test_flat_course_at_152(portlib):
    st, eax = _flat(portlib)
    try:
        o = objs(portlib)
        img = st.image()
        assert eax == struct.unpack_from("<I", img, o["g_flags"][0])[0]
        types = _grid(st, "g_tileType", N * N)
        levels = _grid(st, "g_cornerLevel", (N + 1) ** 2)
        assert set(levels) == {4}
        owned = _owned(types, N)
        xs, ys = {x for x, _ in owned}, {y for _, y in owned}
        side = max(xs) - min(xs) + 1
        # the owned square in the original's proportion: at 50, acres 40 clears count50 rows a side
        with new_state(portlib) as st50:
            st50.load_image(bytes(_fixed_image(portlib, n=COURSE, acres=40)))
            st50.call("generateFlatCourse", [COURSE])
            t50 = _grid(st50, "g_tileType", 2500)
        o50 = _owned(t50, 50)
        side50 = max(x for x, _ in o50) - min(x for x, _ in o50) + 1
        assert abs(side / N - side50 / 50) < 0.02, (side, side50)
        assert max(ys) - min(ys) + 1 == side
        # the clubhouse at the centre, the camera beside it
        hole19 = o["g_holes"][0] + 19 * 0x208
        cx, cy = struct.unpack_from("<ii", img, hole19 + 8)
        assert (cx, cy) == (N // 2 - 2, N // 2 - 2)
        assert struct.unpack_from("<ii", img, o["gCameraTileX"][0])[0] == cx + 6
        # the four corners are for sale
        for x, y in ((0, 0), (0, N - 1), (N - 1, 0), (N - 1, N - 1)):
            assert types[x * N + y] == 0x14
        # g_tileTypeSaved holds the base type under the for-sale tiles
        saved = _grid(st, "g_tileTypeSaved", N * N)
        assert saved[(N - 1) * N + N - 1] == types[cx * N + cy + 10]
    finally:
        st.__exit__(None, None, None)


@need_exe
def test_flat_course_negctl_reads_red(portlib):
    prev = portlib.negctl("generateFlatCourse", 1)
    try:
        st, _ = _flat(portlib)
        owned = _owned(_grid(st, "g_tileType", N * N), N)
        st.__exit__(None, None, None)
    finally:
        portlib.negctl("generateFlatCourse", prev or 0)
    st, _ = _flat(portlib)
    good = _owned(_grid(st, "g_tileType", N * N), N)
    st.__exit__(None, None, None)
    assert len(owned) != len(good)


@need_exe
def test_far_lot_hole_and_walk(portlib):
    st, _ = _flat(portlib)
    try:
        L = (N - 2) // 3
        tva = st.grid_va("g_tileType")
        types = _grid(st, "g_tileType", N * N)
        far = (N - 2, N - 2)
        assert types[far[0] * N + far[1]] == 0x14
        # buy the far lot (sel 8: rows 2 L + 1 .., columns 2 L + 1 ..): every for-sale tile of it becomes its saved type
        n = st.call("buyLotTiles", [8]).eax
        types = _grid(st, "g_tileType", N * N)
        assert n == sum(1 for x in range(2 * L + 1, 3 * L + 1) for y in range(2 * L + 1, 3 * L + 1)
                        if _grid(st, "g_tileTypeSaved", N * N)[x * N + y] != 0x14) or n > 0
        assert types[far[0] * N + far[1]] != 0x14
        # the hole at the far corner: a green tile (type 1) and the tee near it
        st.mem_write(tva + far[0] * N + far[1], b"\x01")
        st.mem_write(tva + (far[0] - 3) * N + far[1] - 3, b"\x02")
        # a golfer walks from the clubhouse to the green: pathStep gives the direction each step (world units, 0x400 a tile)
        o = objs(portlib)
        img = st.image()
        dx = struct.unpack_from("<8i", img, o["f_tab_dirDx_4c2878"][0])
        dy = struct.unpack_from("<8i", img, o["f_tab_dirDy_4c2898"][0])
        x, y = N // 2, N // 2
        steps = 0
        while (x, y) != far and steps < 4 * N:
            # pathStep(target x, target y, walker x, walker y, golfer)
            d = st.call("pathStep", [(far[0] << 10) + 0x200, (far[1] << 10) + 0x200, (x << 10) + 0x200, (y << 10) + 0x200, 0]).eax & 7
            x, y = x + dx[d], y + dy[d]
            assert 0 <= x < N and 0 <= y < N, (x, y)
            steps += 1
        assert (x, y) == far, (x, y, steps)
        # the path grid lives in the side storage: the wave's costs beyond row 127 are there
        pg = _grid(st, "g_pathGrid", N * N)
        assert any(pg[i] for i in range(130 * N, N * N))
    finally:
        st.__exit__(None, None, None)
