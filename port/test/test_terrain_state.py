"""The state side of the port's Terrain.dll (phase 5 step 3, re/PHASE5_TERRAIN_PLAN.md section 13; portsim/sim/terrain_state.h).

Judged against snapshots of the ORIGINAL's Terrain object and module arrays (corpus c7, re/tools/gl_corpus.py, exported to
port/corpus/gl/c7 by port.test.glcorpus):

- construction: the port's Terrain::Terrain + GL setup + buildArrays over a block filled like the debug CRT's operator new must give
  every byte the constructor, the setup and buildArrays determine: the header (type names, counts, flags, window size, zoom), each
  tile's position, neighbour links, face vertex indices and coordinate selectors, the bytes no code writes (0xcd), and the vertex
  grid's x / z. What later calls write (types, heights, textures, walls, paths, the centre heights) is not compared here.
- normals: over the snapshot's own vertices, the port's Tile::calcNormals for every tile the original had recomputed (not dirty in
  the snapshot) must give the snapshot's normal array bit for bit and every face normal by value. The only byte differences
  allowed are the sign of zero on faces the original never recomputed since Faces::build (it holds (+0, 1, +0); a recompute of a
  flat face gives -0): which faces those are is call history, reproduced only when the port replays the exe's calls (plan step 4).
- negative controls: each deliberately wrong body (terrain_state.cpp modes) reads RED on one of these checks.

    ~/bin/py -3.12 -m pytest port/test/test_terrain_state.py -q
"""
from __future__ import annotations

import ctypes as C
import struct

import numpy as np
import pytest

from port.test import glcorpus as G

pytestmark = pytest.mark.port

ROOT = G.ROOT
CORPUS = G.CORPUS / "c7"
INSTALL = ROOT / "original"
NV = G.N_VERTICES
TILES, TILE, OBJ = 0x3A4, 0x248, 0x164AD0
SNAPS = G.snaps(CORPUS)
# controls that construction / buildArrays reveal, and those the normals recompute reveals
NEGCTL_CONSTRUCT = (1, 2, 3)
NEGCTL_NORMALS = (4, 5, 6, 7)
# wrong bodies these checks cannot see, on purpose: Tile::reset's default type is overwritten by the exe's setType on every tile
# before any snapshot (the step-3 captures around single calls must judge it)
NEGCTL_BLIND = {11: "Tile::reset default type 5"}


@pytest.fixture(scope="module")
def lib():
    from port.test import lib as L
    return L.load()


@pytest.fixture(autouse=True)
def _reset(lib):
    bind(lib.dll)
    lib.dll.sg_ts_negctl(0)
    yield
    lib.dll.sg_ts_negctl(0)


def bind(dll):
    G.bind(dll)
    P, U32, I32 = C.c_void_p, C.c_uint32, C.c_int32
    dll.sg_ts_op.restype = C.c_int
    dll.sg_ts_op.argtypes = [P, C.c_char_p, U32, I32, I32, I32, C.POINTER(I32)]
    dll.sg_ts_arrays.restype, dll.sg_ts_arrays.argtypes = C.c_int, [P, P, P, U32]
    dll.sg_ts_textures.restype, dll.sg_ts_textures.argtypes = C.c_int, [P, P, U32]
    dll.sg_ts_set_collar.restype, dll.sg_ts_set_collar.argtypes = C.c_int, [P, P, I32]
    dll.sg_ts_set_course_type.restype, dll.sg_ts_set_course_type.argtypes = C.c_int, [P, I32]
    dll.sg_ts_negctl.restype, dll.sg_ts_negctl.argtypes = C.c_int, [I32]


def op(lib, st, name: str, t: int, a0: int = 0, a1: int = 0, a2: int = 0) -> int:
    r = C.c_int32(0)
    code = lib.dll.sg_ts_op(st.ctx, name.encode(), t, a0, a1, a2, C.byref(r))
    if code:
        raise RuntimeError(f"sg_ts_op {name} {code}")
    return r.value


def arrays(lib, st):
    v, n = (C.c_float * (NV * 3))(), (C.c_float * (NV * 3))()
    assert lib.dll.sg_ts_arrays(st.ctx, v, n, NV) == 0
    return np.frombuffer(bytes(v), "<u4").reshape(-1, 3), np.frombuffer(bytes(n), "<u4").reshape(-1, 3)


def snap_arrays(snap: G.Snap):
    o = lambda rva: rva - G.DATA_RVA0
    return (np.frombuffer(snap.tdata, "<u4", NV * 3, o(G.RVA["vertices"])).reshape(-1, 3),
            np.frombuffer(snap.tdata, "<u4", NV * 3, o(G.RVA["normals"])).reshape(-1, 3))


def empty_globals(lib, st) -> list:
    """Globals with zeroed vertex / normal arrays of the original's size (what the DLL's bss holds before buildArrays)."""
    g = G.SgGlGlobals()
    keep = [(C.c_float * (NV * 3))(), (C.c_float * (NV * 3))()]
    g.vertices, g.n_vertices = C.addressof(keep[0]), NV
    g.normals, g.n_normals = C.addressof(keep[1]), NV
    assert lib.dll.sg_gl_set_globals(st.ctx, C.byref(g)) == 0
    return keep


# --------------------------------------------------------------------------------------------- construction + buildArrays

def construct(lib, base: int, zoom: int = 0):
    """The port's object from scratch: operator new's fill, Terrain(50, 50), the GL setup from original/, buildArrays.
    Returns (object bytes, vertices, normals)."""
    bind(lib.dll)
    with lib.state() as st:
        st.files_root(str(INSTALL))
        obj = (C.c_uint8 * OBJ)(*([0xCD] * OBJ))
        st.map(base, obj)
        keep = empty_globals(lib, st)  # noqa: F841
        op(lib, st, "construct", base, 50, 50)
        op(lib, st, "gl_setup", base, zoom)
        assert op(lib, st, "build_arrays", base) == 1
        v, n = arrays(lib, st)
        return bytes(obj), v, n


def construction_mask() -> np.ndarray:
    """1 for every byte of the object the constructor, the GL setup and buildArrays determine and no later call changes."""
    m = np.zeros(OBJ, bool)
    m[0x04:0x1C] = True        # the four floats, width, height (+0x1c zoom and +0x00 the context handle: separate checks)
    m[0x20:0x3A4] = True       # window size, flags, padding, type names and counts
    for i in range(2500):
        t = TILES + i * TILE
        for k in (0, 2, 4, 6):  # the even level slots: never written
            m[t + 4 * k:t + 4 * k + 4] = True
        m[t + 0x20:t + 0x24] = True
        m[t + 0x2C:t + 0x48] = True    # x, y, neighbours, face count
        for f in range(8):
            o = t + 0x48 + f * 0x38
            m[o:o + 0x24] = True       # vertex indices and coordinate selectors
            m[o + 0x29:o + 0x2C] = True  # bytes after the piece: never written
        m[t + 0x20F] = True
        m[t + 0x23D:t + 0x240] = True
        m[t + 0x245:t + 0x248] = True
    return m


MASK = construction_mask()


@pytest.fixture(scope="module")
def built(lib):
    if not (INSTALL / "Data" / "Textures" / "Parkland").is_dir():
        pytest.skip("no install with Data/Textures (original/)")
    if not SNAPS:
        pytest.skip("no corpus c7")
    base = G.Snap(CORPUS, SNAPS[0]).terrain
    return base, construct(lib, base)


def construct_diff(obj: bytes, v, n, snap: G.Snap) -> dict:
    a = np.frombuffer(obj, np.uint8)
    b = np.frombuffer(snap.tobj, np.uint8)
    bad = np.nonzero(MASK & (a != b))[0]
    sv, _ = snap_arrays(snap)
    return {"bytes": len(bad), "first": int(bad[0]) if len(bad) else -1,
            "xz": int((v[:, [0, 2]] != sv[:, [0, 2]]).any(axis=1).sum()),
            "y": int((v[:, 1] != 0).sum()), "normals": int((n != np.array([0, 0x3F800000, 0], "<u4")).any(axis=1).sum()),
            "glrc": (obj[:4] != b"\0\0\0\0", snap.tobj[:4] != b"\0\0\0\0")}


@pytest.mark.parametrize("step", SNAPS)
def test_construction_equals_the_original(built, step):
    base, (obj, v, n) = built
    snap = G.Snap(CORPUS, step)
    assert snap.terrain == base, "every c7 snapshot has the object at one address (the neighbour links are addresses)"
    d = construct_diff(obj, v, n, snap)
    assert d["bytes"] == 0, f"{d['bytes']} construction bytes differ, first at +0x{d['first']:x}"
    assert d["xz"] == 0 and d["y"] == 0 and d["normals"] == 0, d
    assert d["glrc"] == (True, True), "the context handle is set after initSystem"


def test_zoom_written_by_the_setup(lib):
    if not (INSTALL / "Data" / "Textures" / "Parkland").is_dir():
        pytest.skip("no install with Data/Textures (original/)")
    for step in ("zoom1", "zoom2", "sandbox"):
        snap = G.Snap(CORPUS, step)
        zoom = struct.unpack_from("<i", snap.tobj, 0x1C)[0]
        obj, _, _ = construct(lib, snap.terrain, zoom)
        assert obj[0x1C:0x20] == snap.tobj[0x1C:0x20], step


@pytest.mark.parametrize("mode", NEGCTL_CONSTRUCT)
def test_negctl_construction(lib, built, mode):
    base, _ = built
    snap = G.Snap(CORPUS, "sandbox")
    bind(lib.dll)
    lib.dll.sg_ts_negctl(mode)
    try:
        obj, v, n = construct(lib, base)
    finally:
        lib.dll.sg_ts_negctl(0)
    d = construct_diff(obj, v, n, snap)
    assert d["bytes"] or d["xz"], f"mode {mode} reads GREEN"


def test_construct_and_tile_reset_leave_the_fill(lib, built):
    """The bytes Tile::reset never writes keep operator new's 0xcd: the wall heights and the odd wall flags of an untouched tile
    (the snapshots' plain tiles show the same)."""
    _, (obj, _, _) = built
    snap = G.Snap(CORPUS, "sandbox")
    t = TILES + (25 + 25 * 50) * TILE
    assert obj[t + 0x210:t + 0x234] == b"\xcd" * 0x24 == snap.tobj[t + 0x210:t + 0x234]
    assert obj[t + 0x234:t + 0x23D] == bytes.fromhex("00cd00cd00cd00cdcd") == snap.tobj[t + 0x234:t + 0x23D]


# ------------------------------------------------------------------------------------------------------------- normals

def recompute(lib, snap: G.Snap):
    """The port's calcNormals over the snapshot's Terrain object and vertices for every tile the original had recomputed. Returns
    (object bytes after, normals)."""
    bind(lib.dll)
    with lib.state() as st:
        tob = (C.c_uint8 * len(snap.tobj)).from_buffer_copy(snap.tobj)
        st.map(snap.terrain, tob)
        keep = G.load(lib, st, snap, map_tobj=False)  # noqa: F841
        dirty = [tob[TILES + i * TILE + 0x244] for i in range(2500)]
        for i in range(2500):
            tob[TILES + i * TILE + 0x244] = 0 if dirty[i] else 1
        for i in range(2500):
            op(lib, st, "calc_normals", snap.terrain + TILES + i * TILE)
        for i in range(2500):
            tob[TILES + i * TILE + 0x244] = dirty[i]
        _, n = arrays(lib, st)
        return bytes(tob), n


def normals_diff(obj: bytes, n, snap: G.Snap) -> dict:
    _, sn = snap_arrays(snap)
    never, value = 0, 0
    up = struct.pack("<3f", 0.0, 1.0, 0.0)
    for i in range(2500):
        for f in range(8):
            o = TILES + i * TILE + 0x48 + f * 0x38 + 0x2C
            a, b = snap.tobj[o:o + 12], obj[o:o + 12]
            if a == b:
                continue
            if struct.unpack("<3f", a) == struct.unpack("<3f", b) and a == up:
                never += 1
            else:
                value += 1
    return {"normals": int((n != sn).any(axis=1).sum()), "faces_value": value, "faces_never_recomputed": never}


@pytest.mark.parametrize("step", SNAPS)
def test_normals_equal_the_original(lib, step):
    snap = G.Snap(CORPUS, step)
    obj, n = recompute(lib, snap)
    d = normals_diff(obj, n, snap)
    assert d["normals"] == 0 and d["faces_value"] == 0, d
    # nothing but face normals changes
    a, b = np.frombuffer(obj, np.uint8).copy(), np.frombuffer(snap.tobj, np.uint8).copy()
    for i in range(2500):
        for f in range(8):
            o = TILES + i * TILE + 0x48 + f * 0x38 + 0x2C
            a[o:o + 12] = b[o:o + 12]
    assert (a == b).all()


@pytest.mark.parametrize("mode", NEGCTL_NORMALS)
def test_negctl_normals(lib, mode):
    bind(lib.dll)
    red = []
    for step in ("sandbox", "edit_raise"):
        snap = G.Snap(CORPUS, step)
        lib.dll.sg_ts_negctl(mode)
        try:
            obj, n = recompute(lib, snap)
        finally:
            lib.dll.sg_ts_negctl(0)
        d = normals_diff(obj, n, snap)
        red.append(d["normals"] + d["faces_value"])
    assert any(red), f"mode {mode} reads GREEN"


# ---------------------------------------------------------------------------------------------------------- face textures
# Corpus t1 (re/tools/terrain_calls.py, port.test.terraincorpus): snapshots with the collar table, which c7 lacks.
from port.test import terraincorpus as TC  # noqa: E402

T1 = TC.CORPUS / "t1"
T1_VIEWS = TC.views(T1)
NEGCTL_TEXTURES = (8, 9, 10)


def reblend(lib, snap: "TC.TSnap"):
    """Over the snapshot's own state: resetFaceTextures on every type 0 / 9 tile (their faces take no blend), then
    blendFaceTextures(-1) on every tile. Returns the object bytes after."""
    bind(lib.dll)
    with lib.state() as st:
        tob = (C.c_uint8 * len(snap.tobj)).from_buffer_copy(snap.tobj)
        st.map(snap.terrain, tob)
        keep = G.load(lib, st, snap, map_tobj=False)  # noqa: F841
        col = snap.collar_ints()
        tbl = (C.c_int32 * len(col))(*col)
        assert lib.dll.sg_ts_set_collar(st.ctx, tbl, len(col)) == 0
        assert lib.dll.sg_ts_set_course_type(st.ctx, snap.course_type) == 0
        tiles = [snap.terrain + TILES + i * TILE for i in range(2500)]
        types = [struct.unpack_from("<i", snap.tobj, TILES + i * TILE + 0x24)[0] for i in range(2500)]
        for t, ty in zip(tiles, types):
            if ty in (0, 9):
                op(lib, st, "reset_face_textures", t)
        for t in tiles:
            op(lib, st, "blend", t, -1)
        return bytes(tob)


def texture_diff(obj: bytes, snap) -> dict:
    a, b = np.frombuffer(obj, np.uint8), np.frombuffer(snap.tobj, np.uint8)
    bad = np.nonzero(a != b)[0]
    return {"bytes": len(bad), "first": int(bad[0]) if len(bad) else -1}


def _need_t1():
    if not T1_VIEWS:
        pytest.skip("no corpus t1 (re/tools/terrain_calls.py --label t1; python -m port.test.terraincorpus export ...)")


@pytest.mark.parametrize("step", T1_VIEWS or ["none"])
def test_face_textures_equal_the_original(lib, step):
    _need_t1()
    snap = TC.TSnap(T1, step)
    d = texture_diff(reblend(lib, snap), snap)
    assert d["bytes"] == 0, f"{d['bytes']} bytes differ, first at +0x{d['first']:x}"


@pytest.mark.parametrize("mode", NEGCTL_TEXTURES)
def test_negctl_textures(lib, mode):
    _need_t1()
    lib.dll.sg_ts_negctl(mode)
    try:
        red = [texture_diff(reblend(lib, TC.TSnap(T1, s)), TC.TSnap(T1, s))["bytes"] for s in ("sandbox", "terrain_tools")]
    finally:
        lib.dll.sg_ts_negctl(0)
    assert any(red), f"mode {mode} reads GREEN"


@pytest.mark.parametrize("step", T1_VIEWS or ["none"])
def test_construction_equals_the_original_t1(lib, step):
    """The construction check on the second recording: another object address, zoom 1 / 2 states, a course switch behind it."""
    _need_t1()
    if not (INSTALL / "Data" / "Textures" / "Parkland").is_dir():
        pytest.skip("no install with Data/Textures (original/)")
    snap = TC.TSnap(T1, step)
    zoom = struct.unpack_from("<i", snap.tobj, 0x1C)[0]
    obj, v, n = construct(lib, snap.terrain, zoom)
    d = construct_diff(obj, v, n, snap)
    assert d["bytes"] == 0, f"{d['bytes']} construction bytes differ, first at +0x{d['first']:x}"
    assert d["xz"] == 0 and obj[0x1C:0x20] == snap.tobj[0x1C:0x20], d


@pytest.mark.parametrize("step", T1_VIEWS or ["none"])
def test_normals_equal_the_original_t1(lib, step):
    _need_t1()
    snap = TC.TSnap(T1, step)
    obj, n = recompute(lib, snap)
    d = normals_diff(obj, n, snap)
    assert d["normals"] == 0 and d["faces_value"] == 0, d
