"""P38C: terrain generation at large sizes (re/roadmap/P38_LARGE_MAPS.md phase C; log/roadmap/P38/P38C_notes.md).

Headless, through the C ABI: genNoise, world_resize(N), generateRandomCourse on a course record built from the exe's location
table (course type, setting, relief of each of the 16 locations; acres as seedCourseTable gives record 3). At 101 and 152, for
every location: the owned square in 50's proportion and its border bands for sale (setting 0 / 1) or the island's sea (setting
2), the clubhouse in the window centred at N / 2, inside the owned land and on no water, corner levels in range, every placed
object on the map; the same seed gives the same map and another seed another. The noise lattice does not repeat across a 152
map and keeps 50's cells; the course type 1 falloff band scales. Controls: generateRandomCourse 6 (counts not scaled) and 7
(lengths not scaled), bilinearSample 2 (the lattice wraps at 16), heightBlend 2 (the band stays 16): each reads RED here, and
each is inert at 50 (the N = 50 generator is the original's: test_terrain_gen, its emulator oracle, the segments)."""
from __future__ import annotations

import struct

import pytest

from port.test.test_terrain import need_exe, new_state, objs
from port.test.test_terrain_gen import _fixed_image

pytestmark = pytest.mark.port

COURSE = 3
SIZES = (101, 152)
LOC_REC = 130                        # f_tab_locations_4c1e90 record size (0x82)
LOC_TYPE, LOC_SETTING, LOC_RELIEF = 0x3E, 0x3F, 0x40
ITEM_REC, ITEM_SIDE = 0x14, 0x10     # f_tab_buildItems_4c26b0 records, b10 = the item's side
HOLE_REC = 0x208
WATER = (0x11, 0x12, 0x13)


def _locations(portlib):
    o = objs(portlib)
    with new_state(portlib) as st:
        img = st.image()
    off = o["f_tab_locations_4c1e90"][0]
    return [(img[off + i * LOC_REC + LOC_TYPE], img[off + i * LOC_REC + LOC_SETTING], img[off + i * LOC_REC + LOC_RELIEF])
            for i in range(16)]


def _acres(setting, counter=COURSE):   # seedCourseTable: (counter + 4) 10, + delta (setting 0) / - delta (setting 2)
    base, delta = (counter + 4) * 10, 20 if counter >= 4 else 10
    return base + delta if setting == 0 else base - delta if setting == 2 else base


def _count50(acres):
    count, c = 0, 0x19
    while True:
        count += 1
        c -= 1
        if not c * c * 4 > acres * 10:
            return count


def _generate(portlib, n, loc, ctype, setting, relief, seed=0x1234567, difficulty=1):
    st = new_state(portlib).__enter__()
    img = bytearray(_fixed_image(portlib, n=COURSE, loc=loc, acres=_acres(setting), relief=relief, setting=setting,
                                 ctype=ctype, difficulty=difficulty))
    struct.pack_into("<I", img, objs(portlib)["g_rng"][0], seed)
    st.load_image(bytes(img))
    st.call("genNoise", [])
    if n != 50:
        st.world_resize(n)
    eax = st.call("generateRandomCourse", [COURSE]).eax
    return st, eax


def _grids(st, n):
    return dict(types=st.mem_read(st.grid_va("g_tileType"), n * n),
                levels=st.mem_read(st.grid_va("g_cornerLevel"), (n + 1) ** 2),
                flags=st.mem_read(st.grid_va("g_tileFlags"), 2 * n * n),
                saved=st.mem_read(st.grid_va("g_tileTypeSaved"), n * n))


def _snapshot(st, n):
    return st.image(), tuple(_grids(st, n).values())


def _flag100(g, n):
    return sum(1 for k in range(n * n) if struct.unpack_from("<H", g["flags"], 2 * k)[0] & 0x100)


def _sea_width(g, n):   # setting 1: the widest run of water from column 1 in the saved (pre-sweep) table
    best = 0
    for x in range(n):
        y = 1
        while y < n and g["saved"][x * n + y] == 0x11:
            y += 1
        best = max(best, y - 1)
    return best


@need_exe
@pytest.mark.parametrize("n", SIZES)
def test_every_location_at_n(portlib, n):
    o = objs(portlib)
    side = struct.unpack_from("<b", bytes(new_state(portlib).__enter__().image()), o["f_tab_buildItems_4c26b0"][0] + 15 * ITEM_REC
                              + ITEM_SIDE)[0]
    for loc, (ctype, setting, relief) in enumerate(_locations(portlib)):
        st, eax = _generate(portlib, n, loc, ctype, setting, relief)
        try:
            img = st.image()
            g = _grids(st, n)
            t = g["types"]
            assert eax == struct.unpack_from("<I", img, o["g_flags"][0])[0], loc
            count = (_count50(_acres(setting)) * n + 25) // 50
            # the owned square: 50's proportion (round(count50 N / 50) rows a side), the border bands for sale
            if setting != 2:
                for a in range(count):
                    for s in range(n):
                        assert t[a * n + s] == 0x14 and t[(n - 1 - a) * n + s] == 0x14, (loc, a, s)
                        assert t[s * n + n - 1 - a] == 0x14, (loc, a, s)
                        if setting == 0:
                            assert t[s * n + a] == 0x14, (loc, a, s)
                inner = [(x, y) for x in range(count, n - count) for y in range(count if setting == 0 else 1, n - count)]
                assert all(t[x * n + y] != 0x14 for x, y in inner), loc
                share50 = (50 - 2 * _count50(_acres(setting))) / 50
                assert abs((n - 2 * count) / n - share50) < 0.02, loc
            else:   # the island: at least clamp(|x - N / 2|, 8 N / 50, N) + count - 12 N / 50 cells of sea from each edge
                c2 = count - (12 * n + 25) // 50
                for x in range(n):
                    for a in range(max(abs(x - n // 2), (8 * n + 25) // 50) + c2):
                        for k in (a * n + x, x * n + a, (n - 1 - a) * n + x, x * n + n - 1 - a):
                            assert t[k] == 0x11, (loc, x, a)
            # the clubhouse: in the 17 x 17 window centred at N / 2, on owned land, no water under it
            cx, cy = struct.unpack_from("<ii", img, o["g_holes"][0] + 19 * HOLE_REC + 8)
            lo = n // 2 - 10
            assert lo <= cx < lo + 17 and lo <= cy < lo + 17, (loc, cx, cy)
            for x in range(cx, cx + side):
                for y in range(cy, cy + side):
                    assert t[x * n + y] not in WATER and t[x * n + y] != 0x14, (loc, x, y)
                    assert g["saved"][x * n + y] not in WATER, (loc, x, y)
            assert (struct.unpack_from("<i", img, o["gCameraTileX"][0])[0], struct.unpack_from("<i", img, o["gCameraTileY"][0])[0]) \
                == (cx + 6, cy - 4)
            # corner levels in heightBlend's range, every placed object on the map
            assert 3 <= min(g["levels"]) and max(g["levels"]) <= 15, loc
            po = o["g_placed_objects"][0]
            for i in range(256):
                typ, x, y = struct.unpack_from("<hhh", img, po + 16 * i)
                if typ != -1:
                    assert 0 <= x < n and 0 <= y < n, (loc, i, typ, x, y)
            # the land for sale keeps a generated terrain under it (g_tileTypeSaved), not one type
            assert len(set(g["saved"])) >= 3, loc
        finally:
            st.__exit__(None, None, None)


@need_exe
@pytest.mark.parametrize("n", SIZES)
def test_same_seed_same_map(portlib, n):
    locs = _locations(portlib)
    for loc in (0, 2, 6, 13):   # coast, inland relief 2, island, coast desert
        ctype, setting, relief = locs[loc]
        maps = []
        for seed in (0x1234567, 0x1234567, 0x7654321):
            st, _ = _generate(portlib, n, loc, ctype, setting, relief, seed=seed)
            maps.append(_snapshot(st, n))
            st.__exit__(None, None, None)
        assert maps[0] == maps[1], loc
        assert maps[0][1][0] != maps[2][1][0], loc     # another seed: another tile table


@need_exe
def test_counts_scale_with_the_area_negctl6(portlib):
    ctype, setting, relief = _locations(portlib)[8]
    st, _ = _generate(portlib, 152, 8, ctype, setting, relief)
    good = _flag100(_grids(st, 152), 152)
    st.__exit__(None, None, None)
    prev = portlib.negctl("generateRandomCourse", 6)
    try:
        st, _ = _generate(portlib, 152, 8, ctype, setting, relief)
        bad = _flag100(_grids(st, 152), 152)
        st.__exit__(None, None, None)
    finally:
        portlib.negctl("generateRandomCourse", prev or 0)
    # 50's share of 0x100 tiles at 152 against 50's counts on a 152 map (RED: far fewer)
    st, _ = _generate(portlib, 50, 8, ctype, setting, relief)
    at50 = _flag100(_grids(st, 50), 50)
    st.__exit__(None, None, None)
    assert abs(good / 152 ** 2 - at50 / 2500) < 0.5 * at50 / 2500, (good, at50)
    assert bad < good / 3, (bad, good)


@need_exe
def test_lengths_scale_with_the_side_negctl7(portlib):
    ctype, setting, relief = _locations(portlib)[0]    # a coast: the sea band (2..12 columns at 50)
    assert setting == 1
    st, _ = _generate(portlib, 152, 0, ctype, setting, relief)
    good = _sea_width(_grids(st, 152), 152)
    st.__exit__(None, None, None)
    prev = portlib.negctl("generateRandomCourse", 7)
    try:
        st, _ = _generate(portlib, 152, 0, ctype, setting, relief)
        bad = _sea_width(_grids(st, 152), 152)
        st.__exit__(None, None, None)
    finally:
        portlib.negctl("generateRandomCourse", prev or 0)
    assert good > 20 and bad < 20, (good, bad)


@need_exe
@pytest.mark.parametrize("ctl", [("generateRandomCourse", 6), ("generateRandomCourse", 7), ("bilinearSample", 2),
                                 ("heightBlend", 2)])
def test_controls_are_inert_at_50(portlib, ctl):
    locs = _locations(portlib)
    for loc in (0, 3, 6):
        ctype, setting, relief = locs[loc]
        st, _ = _generate(portlib, 50, loc, ctype, setting, relief)
        good = _snapshot(st, 50)
        st.__exit__(None, None, None)
        prev = portlib.negctl(*ctl)
        try:
            st, _ = _generate(portlib, 50, loc, ctype, setting, relief)
            bad = _snapshot(st, 50)
            st.__exit__(None, None, None)
        finally:
            portlib.negctl(ctl[0], prev or 0)
        assert good == bad, (ctl, loc)


def _bilinear(st, a, b):
    return struct.unpack("<i", struct.pack("<I", st.call("bilinearSample", [a, b]).eax))[0]


@need_exe
def test_noise_lattice_does_not_repeat(portlib):
    ctype, setting, relief = _locations(portlib)[2]
    st, _ = _generate(portlib, 50, 2, ctype, setting, relief)
    pts = [(a, b) for a in range(0x80, 15 * 256 + 0x80, 97) for b in range(0x80, 15 * 256 + 0x80, 89)]
    at50 = [_bilinear(st, a, b) for a, b in pts]
    st.world_resize(152)
    # the map's first 15 cells read the original's lattice
    assert [_bilinear(st, a, b) for a, b in pts] == at50
    # one period (16 cells = 4096 units) further: not the same values
    far = [_bilinear(st, a + 4096, b) for a, b in pts]
    assert sum(1 for u, v in zip(far, at50) if u != v) > len(pts) // 2
    prev = portlib.negctl("bilinearSample", 2)
    try:
        assert [_bilinear(st, a + 4096, b) for a, b in pts] == at50   # RED: the wrap repeats the lattice
    finally:
        portlib.negctl("bilinearSample", prev or 0)
    st.__exit__(None, None, None)


@need_exe
def test_desert_falloff_band_scales(portlib):
    locs = _locations(portlib)
    loc = next(i for i, (t, s, r) in enumerate(locs) if t == 1 and s == 0)
    ctype, setting, _ = locs[loc]
    st, _ = _generate(portlib, 152, loc, ctype, setting, 2)   # relief 2 (g_heightRelief 0x10): the levels leave the floor
    o = objs(portlib)
    # heightBlend at g_date 0 (the generator's own state) with g_flags bit 0 clear (set, it answers 3 everywhere):
    # columns 16..47 lie in the scaled band only
    flags = struct.unpack_from("<I", st.image(), o["g_flags"][0])[0]
    st.write(o["g_flags"][0], struct.pack("<I", flags & ~1))
    assert struct.unpack_from("<i", st.image(), o["g_date"][0])[0] == 0
    hb = lambda x, y: st.call("heightBlend", [x, y]).eax
    x = 76
    near = [hb(x, y) for y in range(16, 48)]
    prev = portlib.negctl("heightBlend", 2)
    try:
        flat = [hb(x, y) for y in range(16, 48)]
    finally:
        portlib.negctl("heightBlend", prev or 0)
    st.__exit__(None, None, None)
    assert sum(near) < sum(flat) - 8, (near, flat)
    assert all(u <= v for u, v in zip(near, flat))


@need_exe
@pytest.mark.parametrize("loc", [2, 6])   # inland at relief 2, the island
def test_generated_152_course_saves_and_loads(portlib, loc):
    """P38D's world save (.sgw) of a generated 152 course: every saved grid, the records and g_sampleGrid (the lattice the
    extended noise derives from) come back, and heightBlend at day 0 answers the same after the load (with g_heightRelief,
    which no save holds, as generated)"""
    from port.test import test_p38d_world_save as W
    n = 152
    o = objs(portlib)
    ctype, setting, relief = _locations(portlib)[loc]
    img = bytearray(W._image(portlib))
    base = o["g_courseTable"][0] + 0x2E * COURSE
    for f, v in ((0x1C, loc), (0x1D, _acres(setting)), (0x1E, relief), (0x1F, setting), (0x20, ctype)):
        img[base + f] = v
    a = new_state(portlib).__enter__()
    try:
        a.load_image(bytes(img))
        a.files_root(None)
        a.call("genNoise", [])
        a.world_resize(n)
        a.call("generateRandomCourse", [COURSE])
        names_a = W.Names(a)
        before = W._snapshot(portlib, a, n)
        noise = a.image()[o["g_sampleGrid"][0]:][:o["g_sampleGrid"][1]]
        pts = [(x, y) for x in range(0, n, 7) for y in range(0, n, 5)]
        hb_before = [a.call("heightBlend", [x, y]).eax for x, y in pts]
        relief_word = a.image()[o["g_heightRelief"][0]:][:4]
        assert W._save(a, names_a, "Gen") == 0
        raw = a.files_read("Saved Games/Gen.sgw")
    finally:
        a.__exit__(None, None, None)
    assert raw is not None and raw[:8] == W.MAGIC
    b = new_state(portlib).__enter__()
    try:
        b.load_image(W._image(portlib))
        b.files_root(None)
        b.files_put("Saved Games/Gen.sgw", raw)
        W._finish_load(b, W.Names(b), "Gen.sgw")
        after = W._snapshot(portlib, b, n)
        assert W._diff(before, after) == []
        assert b.image()[o["g_sampleGrid"][0]:][:o["g_sampleGrid"][1]] == noise
        assert struct.unpack_from("<i", b.image(), o["g_date"][0])[0] == 0
        # g_heightRelief is not in the save's field list (nor in the original's .sve): set as generated, so the comparison
        # judges the noise path (tile types, corner levels, the record, g_sampleGrid)
        b.write(o["g_heightRelief"][0], relief_word)
        assert [b.call("heightBlend", [x, y]).eax for x, y in pts] == hb_before
    finally:
        b.__exit__(None, None, None)
