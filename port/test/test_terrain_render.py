"""Phase 5 step 3 (re/PHASE5_TERRAIN_PLAN.md) and roadmap step S3.1: the port's terrain renderer against recorded passes of the
original.

Each corpus step (re/tools/gl_corpus.py or re/tools/gl_record.py --scenario, exported with `port.test.glcorpus export`) holds one
draw pass of the original Terrain.dll code: its GL calls, the Terrain object and the DLL data it read, and the exported call that
started it (entry kind 1 stripRender, 2 Terrain::render, 3 localRender, 4 pathUpdateRender). The port runs the same call over the
same object and data, and its GL call stream must equal the recorded one line for line (floats as bit patterns).

  - kind 2 (Terrain::render): corpus c7, as before.
  - kind 3 (localRender): every `*_local` / localRender pass of c3 .. c7 (18) and the organic ones of the S3.1 recordings g2 .. g4.
  - kind 1 / 4 (stripRender, pathUpdateRender): the S3.1 recordings g2 (cov_b_holes18, injected after each checkpoint, plus its
    one organic stripRender), g3 (cov_b_paths, every direction at four angles, the zoom shift forced to 1 / 2) and g4
    (cov_b_holes18, injected at the first pass with zoom shift 2), g5 / g6 (cov_b_paths, the tiles fuzzed and pivots near the
    grid edges; g6 makes most tiles type 7 so the border column is not hidden): log/roadmap/S3/1_notes.md.
The draw-list passes are also rasterised: the port's renderer drives GlRaster over the recorded state stream and starting back
buffer, and the frame must meet the G2 criterion (glraster.MAX_ONE_LEVEL of the drawn pixels one level off, at most MAX_BIG more).

Negative controls (portsim/sim/terrain_render.cpp, sg_gl_negctl): each deliberately wrong body must read RED on at least one pass.
"""
from __future__ import annotations

import os

import pytest

from port.test import glcorpus as G
from port.test import glraster as R
from port.test import lib as L

pytestmark = pytest.mark.port

STEPS = G.snaps(kind=G.ENTRY_RENDER)
LOCAL = G.all_snaps({G.ENTRY_LOCAL})
STRIP = G.all_snaps({G.ENTRY_STRIP})
PATH = G.all_snaps({G.ENTRY_PATH})
DRAW_LIST = LOCAL + STRIP + PATH

# negctl mode -> what it breaks
NEGCTL = {1: "drawSkirts keeps its depth", 2: "Tile::render binds every face's texture", 3: "isCulled swaps 90 / 180",
          4: "drawWalls swaps the fence textures", 5: "drawPaths half-width 16.0", 6: "render walks x upward at angle 0",
          7: "drawCornerOverlays swaps masks 5 and 9", 8: "Tile::render rotates type-7 textures one quadrant too far"}
# the draw-list passes' controls (S3.1): judged on the kind 1 / 3 / 4 passes
NEGCTL_LIST = {9: "the draw list rendered in reverse order", 10: "the draw list's second tile skipped",
               11: "stripRender's outer loops start one row later (wrong strip window)",
               12: "pathUpdateRender clips its left edge at x instead of x + 1",
               13: "drawTileObjects follows no wall", 14: "drawTile keeps duplicates"}
# how many of the passes must read RED for each list control (a floor measured on the corpus, so a control that turns
# equivalent on part of it is noticed): see log/roadmap/S3/1_notes.md
NEGCTL_LIST_SCOPE = {9: DRAW_LIST, 10: DRAW_LIST, 11: STRIP, 12: PATH, 13: DRAW_LIST, 14: DRAW_LIST}


@pytest.fixture(scope="module")
def lib():
    return L.load()


@pytest.fixture(autouse=True)
def _reset(lib):
    G.bind(lib.dll)
    lib.dll.sg_gl_negctl(0)
    yield
    lib.dll.sg_gl_negctl(0)


def _skip_without_corpus():
    if not STEPS:
        pytest.skip("no terrain corpus under port/corpus/gl/c7 (python -m port.test.glcorpus export log/p5/gl/corpus/c7 c7)")


def _skip_without(steps, what):
    if not steps:
        pytest.skip(f"no {what} passes under port/corpus/gl (log/roadmap/S3/1_notes.md: export c3 .. c6 kind 3, g2 .. g4 kinds 1,3,4)")


def _id(p):
    return f"{p[0]}/{p[1]}" if isinstance(p, tuple) else str(p)


def _same(lib, label, step):
    snap = G.Snap(G.CORPUS / label, step)
    try:
        got = G.render(lib, snap)
    except RuntimeError as e:      # a fault is a difference too
        return snap, (0, str(e), "")
    return snap, G.first_difference(got, snap.expected)


@pytest.mark.parametrize("step", STEPS or ["-"])
def test_render_stream_equals_the_original(lib, step):
    _skip_without_corpus()
    snap = G.Snap(G.CORPUS / "c7", step)
    got = G.render(lib, snap)
    d = G.first_difference(got, snap.expected)
    assert d is None, f"{step}: first difference at line {d[0]}: port {d[1]!r} original {d[2]!r} ({len(got)} vs {len(snap.expected)} lines)"


@pytest.mark.parametrize("pas", DRAW_LIST or ["-"], ids=_id)
def test_draw_list_stream_equals_the_original(lib, pas):
    _skip_without(DRAW_LIST, "draw-list")
    label, step = pas
    snap, d = _same(lib, label, step)
    assert d is None, f"{label}/{step} (kind {snap.kind}): first difference at line {d[0]}: port {d[1]!r} original {d[2]!r}"


def test_corpus_has_enough_distinct_passes():
    _skip_without_corpus()
    texts = {tuple(G.Snap(G.CORPUS / "c7", s).expected) for s in STEPS}
    assert len(texts) >= 10, f"only {len(texts)} distinct Terrain::render passes in the corpus"


def test_draw_list_corpus_covers_every_entry_angle_and_zoom():
    """The draw-list passes cover the branches the controls and the source reading name: localRender at all four angle arms,
    stripRender in every direction 1..8 at zoom shift 0 / 1 / 2 and at the four angles, pathUpdateRender at the four angles and
    zoom shifts 0 / 1 / 2 (a missing one means a recording was not exported)."""
    _skip_without(DRAW_LIST, "draw-list")
    angles = {0x0, 0x42B40000, 0x43340000, 0xC2B40000}
    seen = {1: set(), 3: set(), 4: set()}
    dirs = {z: set() for z in (0, 1, 2)}
    for label, step in DRAW_LIST:
        snap = G.Snap(G.CORPUS / label, step)
        zoom = int.from_bytes(snap.tobj[0x1C:0x20], "little")
        angle = snap.meta.get("entry_a1" if snap.kind == G.ENTRY_PATH else "entry_a2", 0)
        seen[snap.kind].add((angle, zoom))
        if snap.kind == G.ENTRY_STRIP:
            dirs[zoom].add(snap.meta.get("entry_a1"))
    assert {a for a, _z in seen[3]} >= angles, f"localRender angles {sorted(map(hex, {a for a, _ in seen[3]}))}"
    for k in (1, 4):
        assert {a for a, _z in seen[k]} >= angles, f"kind {k} angles"
        assert {z for _a, z in seen[k]} >= {0, 1, 2}, f"kind {k} zooms {sorted({z for _a, z in seen[k]})}"
    assert dirs[0] >= set(range(1, 9)) and dirs[1] >= set(range(1, 9)), f"strip directions {dirs}"
    assert dirs[2], "no stripRender pass at zoom shift 2"


# ---- the draw-list passes rasterised: the port's own stream into GlRaster, against the original's back buffer ----
_SETUPS: dict = {}


def _setup(label):
    if label not in _SETUPS:
        _SETUPS[label] = R.Setup(G.CORPUS / label)
    return _SETUPS[label]


def _raster(lib, label, step):
    post, got, st, pre = R.run_chain(lib, _setup(label), step)
    rep = R.report(post, got, pre)
    return rep["diff"] - rep["gt1"], rep["gt1"], max(st[2], 1), rep


# every pass by default; SG_S31_RASTER=sample keeps one in four of the strip passes (the largest share of the run time)
RASTER = DRAW_LIST if os.environ.get("SG_S31_RASTER") != "sample" else LOCAL + PATH + STRIP[::4]
# g6 is a constructed state (60 % of the tiles made type 7, the border column not hidden: control 12's recording). GlRaster missed
# the criterion on 16 of its 28 passes until the generic GL's specular table and tie rule were modelled (U-1300 resolved,
# log/roadmap/S3/1b_notes.md); no pass is expected to fail now.
RASTER_OPEN: set = set()


def _raster_params(steps):
    return [pytest.param(p, marks=pytest.mark.xfail(reason="U-1300: GlRaster on the type-7-heavy fuzz state", strict=False))
            if p[0] in RASTER_OPEN else p for p in steps]


@pytest.mark.parametrize("pas", _raster_params(RASTER) or ["-"], ids=_id)
def test_draw_list_raster_meets_the_frame_criterion(lib, pas):
    _skip_without(DRAW_LIST, "draw-list")
    label, step = pas
    one, big, drawn, rep = _raster(lib, label, step)
    assert big <= R.MAX_BIG, f"{label}/{step}: {big} pixels more than one level off (max channel error {rep['max']})"
    assert one <= R.MAX_ONE_LEVEL * drawn, f"{label}/{step}: {one} one-level pixels of {drawn} drawn ({100.0 * one / drawn:.2f} %)"


@pytest.mark.parametrize("mode", sorted(NEGCTL))
def test_negative_control_is_red_somewhere(lib, mode):
    _skip_without_corpus()
    red = []
    lib.dll.sg_gl_negctl(mode)
    for step in STEPS:
        snap = G.Snap(G.CORPUS / "c7", step)
        try:
            got = G.render(lib, snap)
        except RuntimeError:
            red.append(step)       # a fault is a difference too
            continue
        if G.first_difference(got, snap.expected) is not None:
            red.append(step)
    assert red, f"control {mode} ({NEGCTL[mode]}) read GREEN on every pass: the corpus cannot see it"


@pytest.mark.parametrize("mode", sorted(NEGCTL_LIST))
def test_draw_list_negative_control_is_red(lib, mode):
    scope = NEGCTL_LIST_SCOPE[mode]
    _skip_without(scope, "draw-list")
    lib.dll.sg_gl_negctl(mode)
    red = [p for p in scope if _same(lib, *p)[1] is not None]
    assert red, f"control {mode} ({NEGCTL_LIST[mode]}) read GREEN on every pass: the corpus cannot see it"


def test_draw_list_controls_reach_the_pixels(lib):
    """A stream control is also a picture control: the reversed list (9), the skipped tile (10) and the walls not followed (13)
    break the frame criterion on g3's i061_water_strip (stripRender direction 1 at angle 0, cov_b_paths, 202,951 pixels
    changed). Measured (log/roadmap/S3/1_notes.md): 192 / 329 / 76 pixels more than one level off against 1 for the correct body.
    Control 14 (duplicates) redraws a tile over itself and is an equivalent mutant in the picture; the streams judge it."""
    if ("g3", "i061_water_strip") not in DRAW_LIST:
        pytest.skip("no g3 recording exported")
    for mode in (9, 10, 13):
        lib.dll.sg_gl_negctl(mode)
        one, big, drawn, _rep = _raster(lib, "g3", "i061_water_strip")
        lib.dll.sg_gl_negctl(0)
        assert big > R.MAX_BIG or one > R.MAX_ONE_LEVEL * drawn, f"control {mode} stays within the frame criterion ({big} big)"
