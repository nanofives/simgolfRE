"""Phase 5 step 3 (re/PHASE5_TERRAIN_PLAN.md): the port's Terrain::render against recorded passes of the original.

Each corpus step (re/tools/gl_corpus.py, exported with `port.test.glcorpus export`) holds one draw pass of the original
Terrain.dll code started by Terrain::render: its GL calls, the Terrain object and the DLL data it read. The port runs the same
call over the same object and data, and its GL call stream must equal the recorded one line for line (floats as bit
patterns). The recorded passes also replay bit-exact through Windows' software GL (re/tools/glexact/glreplay.c), so an equal
stream is an equal picture.

Negative controls (portsim/sim/terrain_render.cpp, sg_gl_negctl): each deliberately wrong body must read RED on at least one
pass; the table says which passes can see it.
"""
from __future__ import annotations

import pytest

from port.test import glcorpus as G
from port.test import lib as L

pytestmark = pytest.mark.port

STEPS = G.snaps(kind=G.ENTRY_RENDER)

# negctl mode -> what it breaks
NEGCTL = {1: "drawSkirts keeps its depth", 2: "Tile::render binds every face's texture", 3: "isCulled swaps 90 / 180",
          4: "drawWalls swaps the fence textures", 5: "drawPaths half-width 16.0", 6: "render walks x upward at angle 0",
          7: "drawCornerOverlays swaps masks 5 and 9", 8: "Tile::render rotates type-7 textures one quadrant too far"}


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


@pytest.mark.parametrize("step", STEPS or ["-"])
def test_render_stream_equals_the_original(lib, step):
    _skip_without_corpus()
    snap = G.Snap(G.CORPUS / "c7", step)
    got = G.render(lib, snap)
    d = G.first_difference(got, snap.expected)
    assert d is None, f"{step}: first difference at line {d[0]}: port {d[1]!r} original {d[2]!r} ({len(got)} vs {len(snap.expected)} lines)"


def test_corpus_has_enough_distinct_passes():
    _skip_without_corpus()
    texts = {tuple(G.Snap(G.CORPUS / "c7", s).expected) for s in STEPS}
    assert len(texts) >= 10, f"only {len(texts)} distinct Terrain::render passes in the corpus"


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
