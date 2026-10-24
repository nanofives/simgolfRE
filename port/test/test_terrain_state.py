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
# wrong bodies these per-snapshot checks cannot see: Tile::reset's default type is overwritten by the exe's setType on every tile
# before any snapshot; the replay from construction (test_negctl_replay, every mode 1..21) reads it RED
NEGCTL_BLIND = {11: "Tile::reset default type 5 (RED on the replay)"}


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


# ------------------------------------------------------------------------------------------------------- replay (steps 3-4)
# The whole t1 call log replayed from an empty 0xcd block: getInstance (the constructor and the GL setup from original/), then every
# exported call the exe made, in order. At each of the 64 snapshot positions the port's object, vertex and normal arrays, texture
# table and view quadrant must equal the original's.

def bind_dll(dll):
    P, U32, I32 = C.c_void_p, C.c_uint32, C.c_int32
    dll.sg_ts_dll_begin.restype, dll.sg_ts_dll_begin.argtypes = C.c_int, [P, U32]
    dll.sg_ts_call.restype = C.c_int
    dll.sg_ts_call.argtypes = [P, C.c_char_p, U32, C.POINTER(U32), C.POINTER(I32), I32, C.POINTER(U32)]
    dll.sg_ts_view_index.restype, dll.sg_ts_view_index.argtypes = I32, [P]
    dll.sg_run_frames_detail.restype, dll.sg_run_frames_detail.argtypes = C.c_char_p, [P]
    dll.sg_ts_render_data.restype = C.c_int
    dll.sg_ts_render_data.argtypes = [P, P, U32, C.POINTER(U32), P, C.POINTER(U32), C.POINTER(U32), C.POINTER(U32), C.POINTER(U32)]


def state_diff(lib, st, obj, snap) -> dict:
    a = np.frombuffer(bytes(obj), np.uint8).copy()
    b = np.frombuffer(snap.tobj, np.uint8).copy()
    handles = 0
    for off in TC.HANDLE_FIELDS:
        za, zb = a[off:off + 4].any(), b[off:off + 4].any()
        handles += int(za != zb)
        a[off:off + 4] = b[off:off + 4]
    bad = np.nonzero(a != b)[0]
    v, n = arrays(lib, st)
    sv, sn = snap_arrays(snap)
    tex = (C.c_uint32 * G.N_TEXTURES)()
    assert lib.dll.sg_ts_textures(st.ctx, tex, G.N_TEXTURES) == 0
    stex = np.frombuffer(snap.tdata, "<u4", G.N_TEXTURES, G.RVA["textures"] - G.DATA_RVA0)
    view = struct.unpack_from("<i", snap.tdata, G.RVA["view"] - G.DATA_RVA0)[0]
    tiles = sorted({(int(o) - TILES) // TILE for o in bad if TILES <= o < TILES + 2500 * TILE})
    # the renderer's other inputs: the constant tables (read from original/Terrain.dll by getInstance), the wall / fence textures
    # (entries of the table), the screen scale (initSystem's)
    tc, ntc, side = (C.c_float * 256)(), C.c_uint32(), (C.c_float * 24)()
    wall, f0, f1, scale = C.c_uint32(), C.c_uint32(), C.c_uint32(), C.c_uint32()
    assert lib.dll.sg_ts_render_data(st.ctx, tc, 256, C.byref(ntc), side, C.byref(wall), C.byref(f0), C.byref(f1), C.byref(scale)) == 0
    rd = lambda k, n: snap.tdata[G.RVA[k] - G.DATA_RVA0:G.RVA[k] - G.DATA_RVA0 + n]
    render = int(bytes(tc)[:4 * ntc.value] != rd("tex_coords", 4 * G.N_TEXCOORD_FLOATS)) + int(bytes(side) != rd("side_normals", 96))
    render += sum(int(v.value != snap.u32(G.RVA[k])) for v, k in ((wall, "wall"), (f0, "fence0"), (f1, "fence1"), (scale, "scale")))
    return {"render": render, "bytes": len(bad), "first": int(bad[0]) if len(bad) else -1, "tiles": len(tiles), "first_tile": tiles[0] if tiles else -1,
            "handles": handles, "vertices": int((v != sv).any(axis=1).sum()), "normals": int((n != sn).any(axis=1).sum()),
            "textures": int((np.frombuffer(bytes(tex), "<u4") != stex).sum()), "view": int(lib.dll.sg_ts_view_index(st.ctx) != view)}


def run_log(lib, root, stops, on_stop, stop_on_error=True) -> dict:
    """Replay the call log of `root` from an empty 0xcd block; before the first call whose seq is >= a stop position, call
    on_stop(key, st, obj). stops: [(position, key)]. Returns {'_calls', '_rets', '_error'?}."""
    bind(lib.dll)
    bind_dll(lib.dll)
    log = TC.calls(root)
    stops = sorted(stops, key=lambda p: p[0])
    out = {}
    first = log[0]
    assert first.name == "getInstance"
    base = first.ret
    with lib.state() as st:
        st.files_root(str(INSTALL))
        obj = (C.c_uint8 * OBJ)(*([0xCD] * OBJ))
        st.map(base, obj)
        assert lib.dll.sg_ts_dll_begin(st.ctx, base) == 0
        k = 0
        rets = 0
        args = (C.c_uint32 * 5)()
        r = C.c_uint32(0)
        for c in log + [None]:
            while k < len(stops) and (c is None or stops[k][0] <= c.seq):
                on_stop(stops[k][1], st, obj)
                k += 1
            if c is None:
                break
            for i in range(5):
                args[i] = c.args[i]
            tbl, n = None, 0
            if "tbl" in c.extra:
                vals = [int(x) for x in c.extra["tbl"].split(",")]
                tbl, n = (C.c_int32 * len(vals))(*vals), len(vals)
            code = lib.dll.sg_ts_call(st.ctx, c.name.encode(), c.self, args, tbl, n, C.byref(r))
            if code:
                out["_error"] = f"call {c.seq} {c.name}: code {code} {lib.dll.sg_run_frames_detail(st.ctx)}"
                if stop_on_error:
                    break
            if c.name == "getInstance" and r.value != c.ret:
                rets += 1
            if c.name == "render" and (r.value & 0xFF) != (c.ret & 0xFF):
                rets += 1
        out["_calls"] = len(log)
        out["_rets"] = rets
    return out


def replay(lib, root, names, stop_on_error=True) -> dict:
    """Replay the call log once, comparing at each snapshot position; returns {snapshot: diff} plus '_calls' / '_rets' / '_error'."""
    res = {}

    def on_stop(name, st, obj):
        res[name] = state_diff(lib, st, obj, TC.TSnap(root, name))

    out = run_log(lib, root, [(TC.position(TC.TSnap(root, n)), n) for n in names], on_stop, stop_on_error)
    res.update(out)
    return res


# ------------------------------------------------------------------------------------------------------------ the chain
# Corpus t3 (terrain_calls.py --frames --inject): the call log plus, after each scenario step, the original's frame of the last
# Terrain::render pass (gl_corpus form under frames/). The chain: the log replayed up to that render call builds the Terrain object,
# its arrays and texture table; the port's GL setup from original/ sets up the rasteriser at the replayed zoom; the port's
# Terrain::render draws over the original's starting back buffer; the result must meet test_gl_raster's frame criterion. The only
# input still taken from the recording is the starting back buffer (the constant tables come from original/Terrain.dll).
from port.test import glraster as R  # noqa: E402

T3 = TC.CORPUS / "t3"
WALL, FENCE1, FENCE0 = ((G.RVA[k] - G.RVA["textures"]) // 4 for k in ("wall", "fence1", "fence0"))   # entries of the texture table
MAX_ONE_LEVEL, MAX_BIG = R.MAX_ONE_LEVEL, R.MAX_BIG   # the frame criterion (glraster.py, section 18)
# calls that change nothing a Terrain::render pass reads (the view quadrant they set, render sets again from its own angle)
DRAW_ONLY = {"render", "localRender", "stripRender", "pathUpdateRender", "drawLine", "drawCircle", "drawCardinalSpline",
             "drawBezierSpline", "renderTile"}


def frames(root=T3) -> list[str]:
    f = root / "frames" / "index.json"
    import json
    return json.loads(f.read_text()) if f.exists() else []


def frame_render_call(root, name):
    """The Terrain::render call the frame shows. The frame is the last COMPLETED pass when it was fetched, so a render that was still
    running then can follow it in the log: take the latest render with the pass's recorded this / tile / angle among the run of
    draw-only calls just before the fetch position (DRAW_ONLY: every candidate there sees the same state)."""
    d = root / "frames" / "snaps" / name
    pos = int((d / "tcseq.txt").read_text())
    meta = G.meta(d / "meta.txt")
    want = (meta["entry_ecx"], meta["entry_a0"], meta["entry_a1"])
    for c in reversed([c for c in TC.calls(root) if c.seq < pos]):
        if c.name not in DRAW_ONLY:
            break
        if c.name != "render":
            continue
        if (c.self, c.args[0], c.args[1]) == want:
            return c
    raise AssertionError(f"{name}: no render call with the pass's arguments after the last state change")


def chain(lib, root, names, extra=None) -> dict:
    """{frame: (post, got, stats, pre)} for each frame, every frame drawn from the replayed state alone."""
    R.bind(lib.dll)
    G.bind(lib.dll)
    dll = lib.dll
    dll.sg_ts_set_render_data.restype = C.c_int
    dll.sg_ts_set_render_data.argtypes = [C.c_void_p, C.c_void_p, C.c_uint32, C.c_void_p, C.c_uint32, C.c_uint32, C.c_uint32, C.c_uint32]
    dll.sg_gl_render_to_raster.restype = C.c_int
    dll.sg_gl_render_to_raster.argtypes = [C.c_void_p, C.c_void_p, C.c_int32, C.c_uint32, C.c_uint32, C.c_uint32, C.c_uint32]
    dll.sg_gl_setup_to_raster.restype = C.c_int
    dll.sg_gl_setup_to_raster.argtypes = [C.c_void_p, C.c_void_p, C.c_int32, C.POINTER(C.c_uint32)]
    out = {}

    def on_stop(name, st, obj):
        if extra:
            extra(name, st, obj)
        call = frame_render_call(root, name)
        snap = G.Snap(root / "frames", name)
        tex = (C.c_uint32 * G.N_TEXTURES)()
        assert dll.sg_ts_textures(st.ctx, tex, G.N_TEXTURES) == 0
        zoom = struct.unpack_from("<i", bytes(obj), 0x1C)[0]
        r = dll.sg_raster_new(R.W, R.H)
        try:
            table = (C.c_uint32 * G.N_TEXTURES)()
            assert dll.sg_gl_setup_to_raster(st.ctx, r, zoom, table) == 0
            assert list(table) == list(tex), f"{name}: the rasteriser's texture names differ from the replayed table"
            d = root / "frames" / "snaps" / name
            pre = (d / "dib_pre.bin").read_bytes()
            dll.sg_raster_set_pixels(r, pre, R.W * R.H)
            dll.sg_raster_keep_float(r, 1)   # diagnostics: the texel-per-pixel scale of every drawn pixel (the criterion's rho)
            code = dll.sg_gl_render_to_raster(st.ctx, r, 2, call.self, call.args[0], call.args[1], call.args[2])
            assert code == 0, f"{name}: sg_gl_render_to_raster {code}"
            buf = C.create_string_buffer(R.W * R.H * 2)
            dll.sg_raster_get_pixels(r, buf, R.W * R.H * 2)
            stt = (C.c_int64 * 5)()
            dll.sg_raster_stats(r, stt)
            rho = (C.c_float * (R.W * R.H))()
            dll.sg_raster_get_rho(r, rho, R.W * R.H)
            rr = np.frombuffer(bytes(rho), "<f4")
            stats = list(stt) + [float(np.median(rr[rr >= 0])) if (rr >= 0).any() else 0.0]   # [5] = median rho of the drawn pixels
            out[name] = (np.frombuffer((d / "dib_post.bin").read_bytes(), dtype="<u2"), np.frombuffer(buf.raw, dtype="<u2"),
                         stats, np.frombuffer(pre, dtype="<u2"))
        finally:
            dll.sg_raster_free(r)

    res = run_log(lib, root, [(frame_render_call(root, n).seq, n) for n in names], on_stop)
    assert "_error" not in res, res["_error"]
    return out


def judge(post, got, stats, pre):
    """test_gl_raster's frame criterion (glraster.one_level_limit: the same allowance at every texel-per-pixel scale since section 18)."""
    rep = R.report(post, got, pre)
    one = rep["diff"] - rep["gt1"]
    limit = R.one_level_limit(stats[5] if len(stats) > 5 else 0.0)
    return rep["gt1"] <= MAX_BIG and one <= limit * max(stats[2], 1), rep, one, stats[2]


# --------------------------------------------------------------------------------------------------------------- tests
REPLAY_CORPORA = [c for c in ("t1", "t2", "t3") if TC.snaps(TC.CORPUS / c)]
REPLAY_KEYS = [(c, n) for c in REPLAY_CORPORA for n in TC.snaps(TC.CORPUS / c)]
_REPLAYS: dict = {}


def _replay_cached(lib, corpus):
    if corpus not in _REPLAYS:
        root = TC.CORPUS / corpus
        _REPLAYS[corpus] = replay(lib, root, TC.snaps(root))
    return _REPLAYS[corpus]


@pytest.mark.parametrize("corpus,step", REPLAY_KEYS or [("none", "none")])
def test_replay_equals_the_original(lib, corpus, step):
    """The whole call log replayed from construction: at the snapshot's log position the Terrain object (handles as zero /
    nonzero), the vertex and normal arrays, the texture table and the view quadrant equal the original's."""
    if not REPLAY_KEYS:
        pytest.skip("no t1 / t2 / t3 corpus (re/tools/terrain_calls.py; python -m port.test.terraincorpus export)")
    res = _replay_cached(lib, corpus)
    assert "_error" not in res, res["_error"]
    assert res["_rets"] == 0, "getInstance / render returned other values than the original"
    d = res[step]
    assert all(d[k] == 0 for k in ("bytes", "handles", "vertices", "normals", "textures", "view", "render")), d


NEGCTL_REPLAY = tuple(range(1, 23))


@pytest.mark.parametrize("mode", NEGCTL_REPLAY)
def test_negctl_replay(lib, mode):
    """Every deliberately wrong body reads RED on the replay of t2 (the scenario plus the injected calls: type 6 / 7 blocks, paths on
    buildings, filled setWall dwords, the spline step, lowerEdgeCorner at every angle, a Desert course switch)."""
    root = TC.CORPUS / "t2"
    if not TC.snaps(root):
        pytest.skip("no t2 corpus")
    lib.dll.sg_ts_negctl(mode)
    try:
        res = replay(lib, root, TC.snaps(root))
    finally:
        lib.dll.sg_ts_negctl(0)
    red = [k for k, d in res.items() if not k.startswith("_") and any(d[x] for x in ("bytes", "handles", "vertices", "normals",
                                                                                          "textures", "view", "render"))]
    assert red or "_error" in res, f"mode {mode} reads GREEN on the whole replay"


T3_FRAMES = frames()
_CHAINS: dict = {}


def _chain_cached(lib):
    if "t3" not in _CHAINS:
        states = {}

        def keep_state(name, st, obj):
            states[name] = state_diff(lib, st, obj, G.Snap(T3 / "frames", name))

        _CHAINS["state"] = states
        _CHAINS["t3"] = chain(lib, T3, T3_FRAMES, extra=keep_state)
    return _CHAINS


@pytest.mark.parametrize("frame", T3_FRAMES or ["none"])
def test_chain_state_equals_the_frames_snapshot(lib, frame):
    """At the render call the frame shows, the replayed object and arrays equal the snapshot gl_rec.js took of that pass."""
    if not T3_FRAMES:
        pytest.skip("no t3 corpus with frames")
    d = _chain_cached(lib)["state"][frame]
    assert all(d[k] == 0 for k in ("bytes", "handles", "vertices", "normals", "textures", "render")), d


@pytest.mark.parametrize("frame", T3_FRAMES or ["none"])
def test_chain_frame(lib, frame):
    """The frame drawn from the replayed state alone is pixel-identical to the frame drawn from the recorded snapshot (the state
    side adds no error) and meets the frame criterion (U-1076: the same allowance at every zoom since the fixed-point setup, section 18)."""
    if not T3_FRAMES:
        pytest.skip("no t3 corpus with frames")
    if not (INSTALL / "Data" / "Textures" / "Parkland").is_dir():
        pytest.skip("no install with Data/Textures (original/)")
    post, got, stats, pre = _chain_cached(lib)["t3"][frame]
    _, ref, _, _ = R.run_chain_from_setup(lib, R.Setup(T3 / "frames"), frame, INSTALL)
    assert (got == ref).all(), f"{frame}: {int((got != ref).sum())} pixels differ from the snapshot-driven chain"
    ok, rep, one, drawn = judge(post, got, stats, pre)
    assert ok, (rep, one, drawn, "median rho", stats[5])


# --------------------------------------------------------------------------------------- Terrain.dll's constant tables
def _load_constants(lib, files: dict | None):
    bind(lib.dll)
    with lib.state() as st:
        if files is None:
            st.files_root(str(INSTALL))
        else:
            for k, v in files.items():
                st.files_put(k, v)
        r = C.c_int32(0)
        code = lib.dll.sg_ts_op(st.ctx, b"load_dll_constants", 0, 0, 0, 0, C.byref(r))
        return code, r.value


def test_dll_constants_from_the_users_file(lib):
    if not (INSTALL / "Terrain.dll").exists():
        pytest.skip("no original/Terrain.dll")
    assert _load_constants(lib, None) == (0, 1)


def test_dll_constants_refuse_another_file(lib):
    """A Terrain.dll whose texture-coordinate table differs in one byte is refused (MissingResource = SG_E_NO_RESOURCE), and a
    context without one leaves the tables empty."""
    if not (INSTALL / "Terrain.dll").exists():
        pytest.skip("no original/Terrain.dll")
    from port.test import lib as L
    raw = bytearray((INSTALL / "Terrain.dll").read_bytes())
    raw[0x63CA0 + 5] ^= 1
    code, _ = _load_constants(lib, {"Terrain.dll": bytes(raw)})
    assert code == L.SG_E_NO_RESOURCE
    assert _load_constants(lib, {}) == (0, 0)


@pytest.mark.parametrize("mode", (1, 2, 3, 4, 5, 7, 8, "setup"))
def test_criterion_rejects_the_controls_at_zoom2(lib, mode):
    """At zoom 2 the frame criterion rejects the rasteriser's deliberately wrong pipelines (test_gl_raster CONTROLS; 6, blending ignored,
    has nothing to show in these frames: no blended pixel at zoom 2) and the earlier setup model (tune 26 = 1, section 18)."""
    if not T3_FRAMES:
        pytest.skip("no t3 corpus with frames")
    chains = _chain_cached(lib)["t3"]
    setup = R.Setup(T3 / "frames")
    for frame in ("zoom2", "after_inject"):
        def patch(r, dll):
            t = b"#tune 26 1\n" if mode == "setup" else f"#tune 11 {mode}\n".encode()
            dll.sg_raster_exec(r, t, len(t))
        post, got, st, _unknown, pre = R.run(lib, setup, frame, patch=patch)
        ok, rep, one, drawn = judge(post, got, list(st) + [chains[frame][2][5]], pre)
        assert not ok, f"control {mode} meets the criterion on {frame} ({one / max(drawn, 1):.4f} one-level, {rep['gt1']} further)"
