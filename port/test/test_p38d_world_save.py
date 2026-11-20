"""P38D: saving and loading a course larger than 50 (re/roadmap/P38_LARGE_MAPS.md phase D; log/roadmap/P38/P38D_notes.md).

saveGame at N != 50 writes `<name>.sgw`, the port's world save (saveload.cpp "the world save": magic, version, N, the .sve's
header, serializeCourse's field list with every grid at N, the walker-post side table above 127, an end tag); loadGame reads it
by its magic and sets N first (world_resize) before any grid is read. A name has one file: a save of one kind deletes the other
kind's file of that name, and `<name>.sve` asked for loads `<name>.sgw` when only that exists (While_Browsing, the autosaves).
listMatchingFiles adds the world saves to the browser's `Saved Games\\*.sve` listing in the directory's order.

Evidence here (headless, the C ABI): a 152 and a 101 course (P38B's flat course, a far lot bought, greens and tees at the four
far corners, golfers out there, every per-tile statistic grid filled, P38H's land credits in course record byte +0x21, walker
posts above 127) saved, loaded into a fresh state, and compared: N, every grid block at N, every record of the field list; then
saved again, byte-identical to the first file. The same at 50 writes a .sve (no magic, the original's length). Controls
(saveload.worldPost's switch): 1 the grids written at the 50 extent, 2 the posts not read back, 3 a load that does not set N:
each reads RED.
"""
from __future__ import annotations

import ctypes as C
import random
import struct

import numpy as np
import pytest

from port.test.lib import SgError
from port.test.test_terrain import need_exe, new_state, objs
from port.test.test_terrain_gen import _fixed_image

pytestmark = pytest.mark.port

COURSE = 3
NAME_VA = 0x30000000
GREEN, TEE = 1, 2
MAGIC = b"SGPWORLD"
END = b"SGPWEND\0"
HEADER = 0x64
PREFIX = 0x28
# the grids saveGame writes (saveload_fields.inc: the field table's whole-grid ranges), entry size
SAVED_GRIDS = {"g_tileType": 1, "g_tileVariant": 1, "g_tileFlags": 2, "g_tilePathCost": 1, "g_cornerLevel": 1,
               "g_tileMoodUp": 1, "g_tileMoodDown": 1, "g_ballStopCount": 1, "g_tileTypeSaved": 1, "g_tileBuildCode": 1,
               "g_tileRefund": 1}
# records of the field list compared byte for byte after the load (the grids aside)
RECORDS = ("g_holes", "g_golfers", "g_walkers", "g_courseTable", "g_placed_objects", "g_cash_div100", "g_date",
           "g_landPurchaseCount", "g_golferStats", "g_storyPairs", "g_vipResidents", "g_courseRanking", "g_holePoints")
STAT_GRIDS = ("g_tileMoodUp", "g_tileMoodDown", "g_ballStopCount", "g_tileBuildCode", "g_tileRefund")
N_WALKERS = 0x1300 // 0x4C   # sizeof(Walker) = 0x4c: 64 walkers


def _entries(grid, n):
    return (n + 1) ** 2 if grid == "g_cornerLevel" else n * n


def _grid(st, grid, n):
    return st.mem_read(st.grid_va(grid), _entries(grid, n) * SAVED_GRIDS.get(grid, 1))


class Names:
    """a mapped block holding NUL-terminated names for the calls' string arguments"""

    def __init__(self, st):
        self.buf = (C.c_uint8 * 0x1000)()
        st.map(NAME_VA, self.buf)
        self.at = 0

    def put(self, s: str) -> int:
        b = s.encode("latin-1") + b"\0"
        va = NAME_VA + self.at
        C.memmove(C.addressof(self.buf) + self.at, b, len(b))
        self.at += (len(b) + 15) & ~15
        return va


_IMG = {}


def _image(portlib) -> bytes:
    """P38B's fixed course image over the exe's whole data image (saveGame's appendDate reads data the plain fill leaves out)"""
    if "img" not in _IMG:
        with new_state(portlib) as st:
            plain = np.frombuffer(st.image(), np.uint8)
            st.fill_from_exe(whole_image=True)
            whole = np.frombuffer(st.image(), np.uint8).copy()
        fixed = np.frombuffer(bytes(_fixed_image(portlib, n=COURSE, acres=40)), np.uint8)
        changed = fixed != plain
        whole[changed] = fixed[changed]
        _IMG["img"] = whole.tobytes()
    return _IMG["img"]


def _state(portlib, n):
    st = new_state(portlib).__enter__()
    st.load_image(_image(portlib))
    st.files_root(None)
    if n != 50:
        st.world_resize(n)
    st.call("generateFlatCourse", [COURSE])
    return st


def _put(portlib, st, name, data, at=0):
    va = st.grid_va(name) if name in SAVED_GRIDS else None
    if va is None:
        o = objs(portlib)
        img = bytearray(st.image())
        img[o[name][0] + at:o[name][0] + at + len(data)] = data
        st.load_image(bytes(img))
    else:
        st.mem_write(va + at, data)


def _build_course(portlib, st, n, seed=7):
    """the far lot bought, a hole at each far corner, golfers there, statistics everywhere, land credits, flags"""
    rng = random.Random(seed)
    o = objs(portlib)
    L = (n - 2) // 3
    for lot in (0, 2, 6, 8):
        st.call("buyLotTiles", [lot])
    tva = st.grid_va("g_tileType")
    corners = [(1, 1), (1, n - 2), (n - 2, 1), (n - 2, n - 2)]
    for k, (x, y) in enumerate(corners):
        st.mem_write(tva + x * n + y, bytes([GREEN]))
        tx, ty = x + (3 if x < n // 2 else -3), y + (3 if y < n // 2 else -3)
        st.mem_write(tva + tx * n + ty, bytes([TEE]))
    for g in STAT_GRIDS:
        st.mem_write(st.grid_va(g), bytes(rng.randrange(256) for _ in range(n * n)))
    # golfers on the far corners (world units: 0x400 a tile)
    img = bytearray(st.image())
    for k, (x, y) in enumerate(corners):
        struct.pack_into("<ii", img, o["g_golfers"][0] + 0x100 * k, (x << 10) + 0x200, (y << 10) + 0x200)
    # hole records 1..4 hold the corners (+8 x, y as hole 19's clubhouse; the round trip is byte-exact either way)
    for k, (x, y) in enumerate(corners):
        struct.pack_into("<ii", img, o["g_holes"][0] + 0x208 * (k + 1) + 8, x, y)
    struct.pack_into("<i", img, o["g_holesPlusOne"][0], 5)
    # P38H's land credits: byte +0x21 of the course record (g_courseTable + 0x2e * g_courseIndex)
    ci = struct.unpack_from("<i", img, o["g_courseIndex"][0])[0]
    img[o["g_courseTable"][0] + 0x2E * ci + 0x21] = 2
    struct.pack_into("<i", img, o["g_landPurchaseCount"][0], 4)
    struct.pack_into("<i", img, o["g_cash_div100"][0], 123456)
    struct.pack_into("<i", img, o["g_date"][0], 0x2ABCD)
    # the flag word as a load leaves it (bit 30 set, bits 2, 3 and 29 clear), so a second save writes the same word
    flags = struct.unpack_from("<I", img, o["g_flags"][0])[0]
    struct.pack_into("<I", img, o["g_flags"][0], (flags & ~0x2000000C) | 0x40000000)
    st.load_image(bytes(img))
    # walker posts beyond the int8 range (the side table above 127)
    posts = {}
    for w in range(0, N_WALKERS, 7):
        p = (rng.randrange(n), rng.randrange(n))
        st.call("saveload.setWorldPost", [w, 0, p[0]])
        st.call("saveload.setWorldPost", [w, 1, p[1]])
        posts[w] = p
    return posts


def _snapshot(portlib, st, n):
    o = objs(portlib)
    img = st.image()
    recs = {r: bytes(img[o[r][0]:o[r][0] + o[r][1]]) for r in RECORDS}
    grids = {g: _grid(st, g, n) for g in SAVED_GRIDS}
    posts = {w: (st.call("saveload.worldPost", [w, 0]).eax, st.call("saveload.worldPost", [w, 1]).eax) for w in range(N_WALKERS)}
    cam = (struct.unpack_from("<i", img, o["gCameraTileX"][0])[0], struct.unpack_from("<i", img, o["gCameraTileY"][0])[0])
    return {"n": st.world_n(), "recs": recs, "grids": grids, "posts": posts, "camera": cam}


def _save(st, names, stem):
    return st.call("saveGame", [names.put(stem)]).eax


def _load(st, names, fname, finish=0, header_only=0):
    return st.call("loadGame", [names.put(fname), finish, header_only, 0]).eax


def _i16(v):
    v &= 0xFFFF
    return v - 0x10000 if v & 0x8000 else v


def _round_trip(portlib, n):
    a = _state(portlib, n)
    names_a = Names(a)
    try:
        _build_course(portlib, a, n)
        before = _snapshot(portlib, a, n)
        assert _save(a, names_a, "Big") == 0
        ext = ".sve" if n == 50 else ".sgw"
        raw = a.files_read("Saved Games/Big" + ext)
        other = a.files_read("Saved Games/Big" + (".sgw" if n == 50 else ".sve"))
    finally:
        a.__exit__(None, None, None)
    assert raw is not None and other is None
    b = new_state(portlib).__enter__()
    names_b = Names(b)
    try:
        b.load_image(_image(portlib))
        b.files_root(None)
        b.files_put("Saved Games/Big" + ext, raw)
        _finish_load(b, names_b, "Big" + ext)
        after = _snapshot(portlib, b, n)
        assert _save(b, names_b, "Again") == 0
        raw2 = b.files_read("Saved Games/Again" + ext)
    finally:
        b.__exit__(None, None, None)
    return before, after, raw, raw2


def _same_file(a: bytes, b: bytes, header_at: int) -> bool:
    h = slice(header_at, header_at + HEADER)
    return (len(a) == len(b) and a[:header_at] == b[:header_at] and a[h].split(b"\0")[0] == b[h].split(b"\0")[0]
            and a[header_at + HEADER:] == b[header_at + HEADER:])


def _diff(before, after):
    out = []
    if before["n"] != after["n"]:
        out.append(f"N {before['n']} -> {after['n']}")
    for g, v in before["grids"].items():
        w = after["grids"][g]
        if v != w:
            i = next((i for i in range(min(len(v), len(w))) if v[i] != w[i]), min(len(v), len(w)))
            out.append(f"{g} at {i} ({len(v)} vs {len(w)} bytes)")
    for r, v in before["recs"].items():
        if v != after["recs"][r]:
            out.append(r)
    if before["n"] != 50 and before["camera"] != after["camera"]:   # a .sve keeps the running game's camera
        out.append(f"camera {before['camera']} -> {after['camera']}")
    if {w: tuple(map(_i16, p)) for w, p in before["posts"].items()} != {w: tuple(map(_i16, p)) for w, p in after["posts"].items()}:
        out.append("walker posts")
    return out


@need_exe
@pytest.mark.parametrize("n", [152, 101])
def test_world_save_round_trip(portlib, n):
    before, after, raw, raw2 = _round_trip(portlib, n)
    # the file: the prefix, the .sve header, the body with the grids at N, the posts (N > 127), the end tag
    assert raw[:8] == MAGIC
    version, fn, prefix, header, nposts, camx, camy, zero = struct.unpack_from("<8i", raw, 8)
    assert (version, fn, prefix, header, zero) == (1, n, PREFIX, HEADER, 0)
    assert (camx, camy) == before["camera"]
    assert nposts == (2 * N_WALKERS if n > 127 else 0)
    assert raw[-8:] == END
    assert raw[PREFIX:PREFIX + HEADER].split(b"\0")[0] != b""
    # the loaded state is the saved one: N, every grid at N, the records (holes, golfers, walkers, the course table with
    # the land credits), the walker posts
    assert _diff(before, after) == []
    assert after["n"] == n
    ci = 0x2E * COURSE + 0x21
    assert after["recs"]["g_courseTable"][ci] == 2
    # the far corners: greens, and the far lot bought
    t = after["grids"]["g_tileType"]
    for x, y in ((1, 1), (1, n - 2), (n - 2, 1), (n - 2, n - 2)):
        assert t[x * n + y] == GREEN
    assert t[(n - 2) * n + (n - 3)] != 0x14
    if n > 127:
        moved = [w for w, p in after["posts"].items() if max(p) > 127 and max(p) < n]
        assert moved, "no walker post beyond the int8 range"
    # saved again from the loaded state: the same file, byte for byte, but the header's bytes after its NUL (the original
    # writes the 0x64 bytes of g_textBuffer, whose tail holds what the buffer held before: here the path of each save)
    assert _same_file(raw, raw2, PREFIX)


@need_exe
def test_sve_at_50_unchanged(portlib):
    """at 50 the save is the original's .sve (no world file written; test_saveload pins its bytes against the original):
    no prefix, the header first, and the same round trip holds"""
    before, after, raw, raw2 = _round_trip(portlib, 50)
    assert raw[:8] != MAGIC and raw[-8:] != END
    assert _same_file(raw, raw2, 0)
    assert _diff(before, after) == []


@need_exe
def test_one_file_per_name_and_fallback(portlib):
    st = _state(portlib, 152)
    names = Names(st)
    try:
        _build_course(portlib, st, 152)
        assert _save(st, names, "Slot") == 0
        assert st.files_read("Saved Games/Slot.sgw") is not None and st.files_read("Saved Games/Slot.sve") is None
        # the same name at 50 replaces the world file by a .sve
        st.world_resize(50)
        st.call("generateFlatCourse", [COURSE])
        assert _save(st, names, "Slot") == 0
        assert st.files_read("Saved Games/Slot.sve") is not None and st.files_read("Saved Games/Slot.sgw") is None
        # and back
        st.world_resize(152)
        st.call("generateFlatCourse", [COURSE])
        assert _save(st, names, "While_Browsing") == 0
        assert st.files_read("Saved Games/While_Browsing.sgw") is not None
        st.world_resize(50)
        assert st.world_n() == 50
        # the browser's reload asks for While_Browsing.sve: the world file is read, N back to 152
        _load(st, names, "While_Browsing.sve")
        assert st.world_n() == 152
        # a header-only read (the browser's autosave title) reads the header and leaves N alone
        st.world_resize(50)
        assert _load(st, names, "While_Browsing.sgw", finish=0, header_only=1) == 0
        assert st.world_n() == 50
        o = objs(portlib)
        text = st.image()[o["g_textBuffer"][0]:][:HEADER]
        raw = st.files_read("Saved Games/While_Browsing.sgw")
        assert text.split(b"\0")[0] == raw[PREFIX:PREFIX + HEADER].split(b"\0")[0] != b""
    finally:
        st.__exit__(None, None, None)


@need_exe
def test_unknown_world_version_is_refused(portlib):
    st = _state(portlib, 152)
    names = Names(st)
    try:
        assert _save(st, names, "V") == 0
        raw = bytearray(st.files_read("Saved Games/V.sgw"))
        raw[8] = 9   # version 9
        st.files_put("Saved Games/V9.sgw", bytes(raw))
        st.world_resize(50)
        img = st.image()
        assert _load(st, names, "V9.sgw") == 0xFFFFFFFF
        assert st.world_n() == 50
        o = objs(portlib)
        assert st.image()[o["g_holes"][0]:][:0x2000] == img[o["g_holes"][0]:][:0x2000]
    finally:
        st.__exit__(None, None, None)


@need_exe
def test_browser_listing_holds_both_kinds(portlib):
    """listMatchingFiles(`Saved Games\\*.sve`) lists the world saves too, interleaved in name order; with none it is the
    original's listing"""
    st = _state(portlib, 50)
    names = Names(st)
    try:
        for f in ("Saved Games/b.sve", "Saved Games/d.sve", "Saved Games/note.txt"):
            st.files_put(f, b"x" * 200)
        pat = names.put("Saved Games" + chr(92) + "*.sve")
        none = names.put("")
        o = objs(portlib)

        def listed():
            k = st.call("listMatchingFiles", [pat, none, none]).eax
            img = st.image()
            base = o["g_fileNames"][0]
            return k, [img[base + 100 * i:base + 100 * i + 100].split(b"\0")[0].decode() for i in range(k)]

        assert listed() == (2, ["b.sve", "d.sve"])
        st.files_put("Saved Games/a.sgw", b"y" * 200)
        st.files_put("Saved Games/c.sgw", b"y" * 200)
        assert listed() == (4, ["a.sgw", "b.sve", "c.sgw", "d.sve"])
    finally:
        st.__exit__(None, None, None)


@need_exe
@pytest.mark.parametrize("mode,what", [(1, "grids"), (2, "posts"), (3, "N")])
def test_world_save_controls_read_red(portlib, mode, what):
    prev = portlib.negctl("saveload.worldPost", mode)
    try:
        try:
            before, after, raw, raw2 = _round_trip(portlib, 152)
            bad = _diff(before, after)
        except SgError as e:   # a fault counts as RED only for control 3 (a load at the wrong N)
            assert mode == 3, e
            bad = [str(e)]
    finally:
        portlib.negctl("saveload.worldPost", prev or 0)
    assert bad, f"control {mode} ({what}) changed nothing"


# the finish path's callees that need the Terrain or the exe's constants headless, recorded instead (test_saveload's way)
FINISH_PEERS = ("applyCourseType", "loadWorldAssets", "refreshTerrain", "drawWindow", "Window::refreshFull", "rebuildTerrainGrid")
PEER_LOG_VA = 0x32000000


def _finish_load(st, names, fname):
    from port.test.emu38b import fnv1a
    log = (C.c_uint32 * (1 + 8 * 64))()
    st.map(PEER_LOG_VA, log)
    for nm in FINISH_PEERS:
        st.call("commands.recordPeer", [fnv1a(nm), PEER_LOG_VA, 64])
    try:
        return _load(st, names, fname, finish=1)
    finally:
        st.call("commands.recordClear", [])


@need_exe
@pytest.mark.parametrize("mode", [0, 4])
def test_world_load_restores_the_camera(portlib, mode):
    """a finished load of a world file puts the camera where it was saved (a browser preview, finish 0, does not); control 4
    (the camera not restored) reads RED"""
    o = objs(portlib)
    st = _state(portlib, 152)
    names = Names(st)
    prev = portlib.negctl("saveload.worldPost", mode)
    try:
        img = bytearray(st.image())
        struct.pack_into("<ii", img, o["gCameraTileX"][0], 140, 12)
        st.load_image(bytes(img))
        assert _save(st, names, "Cam") == 0
        img = bytearray(st.image())
        struct.pack_into("<ii", img, o["gCameraTileX"][0], 20, 30)
        st.load_image(bytes(img))
        _load(st, names, "Cam.sgw", finish=0)
        cam = lambda: struct.unpack_from("<ii", st.image(), o["gCameraTileX"][0])
        assert cam() == (20, 30)
        _finish_load(st, names, "Cam.sgw")
        assert st.world_n() == 152
        assert (cam() == (140, 12)) == (mode == 0)
    finally:
        portlib.negctl("saveload.worldPost", prev or 0)
        st.__exit__(None, None, None)
