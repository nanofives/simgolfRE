"""Roadmap S3.28 (U-1952): Terrain::lowerEdgeCorner's draw half, renderTileAtFiveOffsets (Terrain.dll 0x10038900, matched in
re/match/terrain_edge.cpp) with enableBlackLight1 (0x10003830, re/match/terrain_small3.cpp), ported as terrain pass kind 5
(portsim/sim/terrain_render.h) and handed to the graphics backend by LiveTerrain::lower_edge_corner before the state call.

Corpus port/corpus/gl/s328_s2b (log/roadmap/S3/28_gl_record.py on cov_r_s2_screens, instance s3_w8, hooks off, the run SAME as the
pinned det_b; exported with `port.test.glcorpus export <src> s328_s2b 5`):
  * k<NN>_lowerEdge_o<ord>: every organic lowerEdgeCorner pass (entry_a0..a3 = tile, corner, pivot, angle); all have angle 0;
  * i<seq>_course_rtafo: renderTileAtFiveOffsets called directly after `course` for the tiles on the four edges of the land
    (x, y = 12..37; every other tile hidden) and one inner tile at the four view angles (entry_direct 1, entry_a0..a2 = tile, pivot,
    angle): each edge tile draws at exactly the two angles whose neighbour set reaches the hidden side, so the three angle tests and
    the else arm are told apart, each with a drawing and a non-drawing pass.
The port's stream must equal the recorded pass line for line, pointer arguments dropped but the light / parameter arguments of
glLightfv kept (glcorpus.normalize drops those too, so this file compares with its own `strict`). Pixels: GlRaster from the port's
stream meets G2 on every pass and the host's own GL (re/tools/glexact/glreplay.exe, a local build; skipped without it) equals the
original's back buffer in every pixel.

Negative controls: sg_gl_negctl 15 (nothing drawn), 16 (second offset -10), 17 (light 1 ambient 0.2: GlRaster models light 0 only,
so its pixels are blind to it; the stream and the host GL are not), 18 (the 90 and 180 degree neighbour sets swapped), and gfx.family.terrain 15 (LiveTerrain hands no kind-5 pass to
the backend) on cov_r_s2_screens `course`, whose map corner holds the silhouette (log/roadmap/S3/27_notes.md: 65 px).

Backends: the stream, raster and host-GL tests call the renderer through the ctypes DLL's sg_gl_* / sg_raster_* entry points
(glcorpus.py, glraster.py), which the Win32 runner does not serve (port/test/lib32.py: no ctypes DLL, no sg_gl op), so with
SG_BACKEND=win32 they skip with that reason. The corpus-file tests and the frame tests (the
map-corner box and its control, through framegate's boot with LiveTerrain) run on both backends.
"""
from __future__ import annotations

import os
import pathlib
import shutil
import subprocess

import numpy as np
import pytest
from PIL import Image

from port.test import glcorpus as G
from port.test import glraster as R
from port.test import lib as L

pytestmark = pytest.mark.port

LABEL = "s328_s2b"
ROOT = G.CORPUS / LABEL
STEPS = G.snaps(ROOT, kind=5)
REPLAY = G.ROOT / "re" / "tools" / "glexact" / "glreplay.exe"
W, H = 800, 600
# the three organic passes that write pixels on the course frame (log/roadmap/S3/27_notes.md: 1,497 / 177 / 26 px), by ordinal
VISIBLE = {3: 1497, 4: 177, 5: 26}
# the map-corner box of cov_r_s2_screens (x0, y0, x1, y1 inclusive; 26_notes.md group 2)
BOX = (781, 451, 799, 466)


@pytest.fixture(scope="module")
def lib():
    return L.load()


@pytest.fixture(autouse=True)
def _reset(lib):
    if lib.dll is None:     # the Win32 runner: no ctypes DLL (see the module docstring)
        yield
        return
    G.bind(lib.dll)
    lib.dll.sg_gl_negctl(0)
    yield
    lib.dll.sg_gl_negctl(0)


def _skip(lib=None):
    if lib is not None and lib.dll is None:
        pytest.skip("the Win32 runner serves no sg_gl_* / sg_raster_* entry point (port/test/lib32.py): the renderer's streams and "
                    "pixels are judged through the x64 DLL")
    if not STEPS:
        pytest.skip(f"no kind-5 recording exported to {ROOT} (log/roadmap/S3/28_gl_record.py, then glcorpus export ... {LABEL} 5)")


def strict(text: str) -> list[str]:
    """The recorder's lines with only the pointer argument of a glXXXfv call dropped (its other arguments and its `v=` kept)."""
    out = []
    for ln in text.splitlines():
        ln = ln.rstrip()
        if not ln:
            continue
        t = ln.split(" ")
        vs = [x for x in t[1:] if x.startswith("v=")]
        if vs:
            args = [x for x in t[1:] if x and not x.startswith("v=")]
            a = args[0].split(",")[:-1] if args else []
            ln = t[0] + (" " + ",".join(a) if a else "") + " " + vs[0]
        out.append(ln)
    return out


def args(snap: G.Snap) -> tuple[int, int, int]:
    """The port's kind-5 arguments (tile, pivot, angle): a direct call's own, lowerEdgeCorner's 0, 2 and 3."""
    m = snap.meta
    if m.get("entry_direct"):
        return m["entry_a0"], m["entry_a1"], m["entry_a2"]
    return m["entry_a0"], m["entry_a2"], m["entry_a3"]


def port_stream(lib, snap: G.Snap) -> list[str]:
    import ctypes as C
    with lib.state() as st:
        keep = G.load(lib, st, snap)  # noqa: F841
        cap = 1 << 24
        out = C.create_string_buffer(cap)
        n = C.c_size_t(0)
        code = lib.dll.sg_gl_render(st.ctx, 5, snap.terrain, *args(snap), out, cap, C.byref(n))
        if code:
            raise RuntimeError(f"sg_gl_render code {code}")
        return out.raw[:n.value].decode().splitlines()


def raster(lib, step: str):
    """(post, got, pre) of the port's renderer driving GlRaster over the recorded state stream and starting back buffer."""
    import ctypes as C
    dll = lib.dll
    R.bind(dll)
    dll.sg_gl_render_to_raster.restype = C.c_int
    dll.sg_gl_render_to_raster.argtypes = [C.c_void_p, C.c_void_p, C.c_int32, C.c_uint32, C.c_uint32, C.c_uint32, C.c_uint32]
    setup = _setup()
    snap = G.Snap(ROOT, step)
    d = ROOT / "snaps" / step
    r = dll.sg_raster_new(W, H)
    try:
        with lib.state() as st:
            keep = G.load(lib, st, snap)  # noqa: F841
            for h, b in setup.textures.items():
                dll.sg_raster_texture(r, h.encode(), b, len(b))
            state = "\n".join(setup.lines[setup.ctx_line + 1:snap.meta["stateIdx"]]).encode()
            dll.sg_raster_exec(r, state, len(state))
            pre = (d / "dib_pre.bin").read_bytes()
            dll.sg_raster_set_pixels(r, pre, W * H)
            code = dll.sg_gl_render_to_raster(st.ctx, r, 5, snap.terrain, *args(snap))
            if code:
                raise RuntimeError(f"sg_gl_render_to_raster {code}")
            buf = C.create_string_buffer(W * H * 2)
            dll.sg_raster_get_pixels(r, buf, W * H * 2)
            post = np.frombuffer((d / "dib_post.bin").read_bytes(), "<u2")
            return post, np.frombuffer(buf.raw, "<u2"), np.frombuffer(pre, "<u2")
    finally:
        dll.sg_raster_free(r)


_SETUP = []


def _setup():
    if not _SETUP:
        _SETUP.append(R.Setup(ROOT))
    return _SETUP[0]


def host_gl(lib, step: str, tmp: pathlib.Path):
    """(post, got) of the port's stream replayed by the host's own GL (glreplay.exe over a corpus copy holding the port's pass)."""
    snap = G.Snap(ROOT, step)
    cr = tmp / "c"
    if not (cr / "stream.gl.txt").exists():
        (cr / "snaps").mkdir(parents=True, exist_ok=True)
        shutil.copyfile(ROOT / "stream.gl.txt", cr / "stream.gl.txt")
        shutil.copytree(ROOT / "tex", cr / "tex", dirs_exist_ok=True)
    sd = cr / "snaps" / step
    sd.mkdir(parents=True, exist_ok=True)
    for f in ("vtx.bin", "nrm.bin", "dib_pre.bin", "dib_post.bin", "meta.txt"):
        shutil.copyfile(snap.dir / f, sd / f)
    (sd / "pass.gl.txt").write_text("\n".join(port_stream(lib, snap)) + "\n")
    ob = tmp / f"{step}.bin"
    subprocess.run([str(REPLAY), str(cr), step, str(ob)], capture_output=True, text=True, timeout=300)
    post = np.frombuffer((snap.dir / "dib_post.bin").read_bytes(), "<u2")
    return post, np.frombuffer(ob.read_bytes(), "<u2")


def _drawn(step: str) -> int:
    d = ROOT / "snaps" / step
    return int((np.frombuffer((d / "dib_post.bin").read_bytes(), "<u2") != np.frombuffer((d / "dib_pre.bin").read_bytes(), "<u2")).sum())


# ---------------------------------------------------------------------------------------------------------------- the streams
@pytest.mark.parametrize("step", STEPS or ["-"])
def test_kind5_stream_equals_the_original(lib, step):
    _skip(lib)
    snap = G.Snap(ROOT, step)
    got = port_stream(lib, snap)
    exp = strict((snap.dir / "pass.gl.txt").read_text())
    d = G.first_difference(got, exp)
    assert d is None, f"{step}: line {d[0]}: port {d[1]!r}, original {d[2]!r}"


def test_corpus_covers_both_arms_at_every_angle():
    """Organic passes from the course frame (the three visible ones among them) and direct calls at angle 0, 90, 180 and -90, each
    angle with a drawing and a non-drawing pass (a non-drawing pass has no glArrayElement)."""
    _skip()
    org = [s for s in STEPS if not G.Snap(ROOT, s).meta.get("entry_direct")]
    assert len(org) >= 12, org
    ords = {G.Snap(ROOT, s).meta.get("entry_ord"): s for s in org}
    for o, n in VISIBLE.items():
        assert o in ords and _drawn(ords[o]) == n, (o, ords.get(o), n)
    arms = {}
    for s in STEPS:
        snap = G.Snap(ROOT, s)
        ang = args(snap)[2]
        draws = any(ln.startswith("glArrayElement") for ln in (snap.dir / "pass.gl.txt").read_text().splitlines())
        arms.setdefault(ang, set()).add(draws)
    for ang in (0, 0x42B40000, 0x43340000, 0xC2B40000):
        assert arms.get(ang) == {True, False}, (hex(ang), arms.get(ang))


def test_direct_tiles_draw_at_exactly_two_angles():
    """Each edge tile draws at two of the four angles and the inner tile at none: the neighbour sets are told apart."""
    _skip()
    by_tile = {}
    for s in STEPS:
        snap = G.Snap(ROOT, s)
        if not snap.meta.get("entry_direct"):
            continue
        tile, _pivot, ang = args(snap)
        draws = any(ln.startswith("glArrayElement") for ln in (snap.dir / "pass.gl.txt").read_text().splitlines())
        by_tile.setdefault(tile, {})[ang] = draws
    counts = sorted(sum(v.values()) for v in by_tile.values())
    assert len(by_tile) == 5 and all(len(v) == 4 for v in by_tile.values()) and counts == [0, 2, 2, 2, 2], by_tile


# ---------------------------------------------------------------------------------------------------------------- the pixels
def _pixel_steps():
    return [s for s in STEPS if (ROOT / "snaps" / s / "dib_post.bin").exists()] if STEPS else []


@pytest.mark.parametrize("step", _pixel_steps() or ["-"])
def test_kind5_raster_meets_the_frame_criterion(lib, step):
    _skip(lib)
    post, got, pre = raster(lib, step)
    rep = R.report(post, got, pre)
    drawn = max(rep["changed"], 1)
    assert rep["gt1"] <= R.MAX_BIG, (step, rep)
    assert rep["diff"] - rep["gt1"] <= R.MAX_ONE_LEVEL * drawn, (step, rep)


@pytest.mark.parametrize("step", _pixel_steps() or ["-"])
def test_kind5_host_gl_equals_the_original(lib, step, tmp_path):
    _skip(lib)
    if not REPLAY.exists():
        pytest.skip("no re/tools/glexact/glreplay.exe (re/tools/glexact/build.bat): the host GL is the reference")
    post, got = host_gl(lib, step, tmp_path)
    assert int((post != got).sum()) == 0, step


# ---------------------------------------------------------------------------------------------------------------- controls
def _red(lib, steps):
    return [s for s in steps if G.first_difference(port_stream(lib, G.Snap(ROOT, s)), strict((ROOT / "snaps" / s / "pass.gl.txt").read_text())) is not None]


@pytest.mark.parametrize("mode", [15, 16, 17])
def test_renderer_controls_are_red_on_the_streams(lib, mode):
    """Every drawing pass reads RED (15 also takes the matrix lines of a non-drawing pass, 16 / 17 act only where it draws)."""
    _skip(lib)
    lib.dll.sg_gl_negctl(mode)
    drawing = [s for s in STEPS if any(ln.startswith("glArrayElement") for ln in (ROOT / "snaps" / s / "pass.gl.txt").read_text().splitlines())]
    red = _red(lib, drawing)
    assert red == drawing, f"control {mode} GREEN on {sorted(set(drawing) - set(red))}"


def test_neighbour_set_control_is_red_on_the_streams(lib):
    """Control 18 (90 / 180 neighbour sets swapped): the four direct edge-tile passes at 90 and 180 whose arm flips read RED."""
    _skip(lib)
    lib.dll.sg_gl_negctl(18)
    direct = [s for s in STEPS if G.Snap(ROOT, s).meta.get("entry_direct") and args(G.Snap(ROOT, s))[2] in (0x42B40000, 0x43340000)]
    red = _red(lib, direct)
    assert len(red) >= 4, (red, direct)


def _visible_step(o: int) -> str:
    for s in STEPS:
        m = G.Snap(ROOT, s).meta
        if not m.get("entry_direct") and m.get("entry_ord") == o:
            return s
    pytest.skip(f"no organic pass o{o}")


@pytest.mark.parametrize("mode", [15, 16])
def test_renderer_controls_reach_the_raster_pixels(lib, mode):
    _skip(lib)
    step = _visible_step(3)
    lib.dll.sg_gl_negctl(mode)
    post, got, pre = raster(lib, step)
    rep = R.report(post, got, pre)
    assert rep["gt1"] > R.MAX_BIG, (mode, rep)


def test_light1_control_reaches_the_host_gl_pixels(lib, tmp_path):
    _skip(lib)
    if not REPLAY.exists():
        pytest.skip("no re/tools/glexact/glreplay.exe")
    step = _visible_step(3)
    lib.dll.sg_gl_negctl(17)
    post, got = host_gl(lib, step, tmp_path)
    assert int((post != got).sum()) > 100, step


# ---------------------------------------------------------------------------------------------------------------- the frame
@pytest.fixture(scope="module")
def glib():
    lib = L.load()
    if not lib.has("gfx.attach") or not lib.has("gditext.attach"):
        pytest.skip("the library has no gfx module / host text provider")
    if not lib.exe_loaded() and lib.exe_load():
        pytest.skip("no exe image (original/golf_clean.exe or SG_EXE_PATH)")
    return lib


def _box(lib, tmp, negctl=None):
    """differing px (any level, beyond one level) of cov_r_s2_screens `course` in BOX against the reference, framegate's boot with
    LiveTerrain and the default renderer (GlRaster)."""
    from port.test import framegate as FG
    from port.test import gfx as GX
    from port.test import segments as SG
    scen, chk = "cov_r_s2_screens", "course"
    if not (GX.REFS / scen / "det_b" / f"{chk}.png").exists() or not SG.INSTALL.is_dir():
        pytest.skip("no pinned reference / install")
    FG.run_scenario(scen, [chk], lib, negctl=negctl, images=tmp, probe=False)
    port = np.array(Image.open(tmp / f"{scen}.{chk}.port.png").convert("RGB")).astype(int) >> 3
    ref = GX.reference(scen, chk).astype(int) >> 3
    x0, y0, x1, y1 = BOX
    d = np.abs(port[y0:y1 + 1, x0:x1 + 1] - ref[y0:y1 + 1, x0:x1 + 1]).max(axis=2)
    return int((d > 0).sum()), int((d > 1).sum())


def test_map_corner_box_equals_the_original(glib, tmp_path):
    """The silhouette is drawn: no pixel of the box beyond one level (GlRaster's terrain residue stays within one level)."""
    _any, big = _box(glib, tmp_path)
    assert big == 0, (_any, big)


def test_map_corner_box_negctl_red(glib, tmp_path):
    """gfx.family.terrain 15: LiveTerrain hands no kind-5 pass over, the port's terrain shows through the corner. Measured: 65 px
    of the box differ, 20 of them beyond one level, the wave 8 census's numbers for this row (27_notes.md: the counterfactual
    differs from the original in 65 px of the box)."""
    _any, big = _box(glib, tmp_path, negctl=("gfx.family.terrain", 15))
    assert _any >= 60 and big >= 15, (_any, big)
