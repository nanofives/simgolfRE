"""Phase 5 step 3 (re/PHASE5_TERRAIN_PLAN.md): the port's software rasteriser against the original's back-buffer frames.

Each corpus snapshot (re/tools/gl_corpus.py, exported with `port.test.glcorpus export`) holds one draw pass of the ORIGINAL
Terrain.dll code and the 800x600 RGB555 back buffer before and after it. The rasteriser replays the state stream and the
pass's GL calls (port/test/glraster.py) onto the same starting buffer; the result is compared with the original's pixel by pixel.

THE CRITERION. Not bit-exact, on purpose and measured: with the fixed-point triangle setup (section 18) the rasteriser writes the
original's pixel in all but 0.47 % of the drawn pixels on average over the 49 passes of c7 and t3 (worst pass 1.05 %), every other
pixel is one 5-bit level off in one channel, and up to 50 pixels per pass differ by more (triangle seams covered on one side only).
So a snapshot passes when the one-level pixels are at most 1.5 % of the drawn pixels and at most 70 pixels differ by more. What the
pipeline reproduces exactly was fitted from the data and from probes against the real software GL (glprobe.py) and is written in
gl_raster.cpp and re/PHASE5_TERRAIN_PLAN.md: the GL_MODULATE lighting (the original's glTexEnvi call names GL_TEXTURE_2D, an
invalid target, so the mode stays modulate), the 4x4 ordered dither and the 31/255 channel scale (a flat-colour probe matches
exactly), glTexParameterf filters (GL_LINEAR, passed as floats) for the path textures, untextured fragments for an incomplete
texture, source-alpha blending, and the triangle setup: vertices scaled by 1 - 2^-8 / h about the viewport centre and rounded to
8 fractional bits, for coverage and attributes, sampled at pixel centres (section 18; `#tune 26 1` restores the linear model).

Negative controls (the `#tune 11 <mode>` knob of GlRaster), each of which must break the criterion on at least one pass.
"""
from __future__ import annotations

import pytest

from port.test import glcorpus as G
from port.test import glraster as R
from port.test import lib as L

pytestmark = pytest.mark.port

ROOT = G.CORPUS / "c7"
STEPS = G.snaps(ROOT)
MAX_ONE_LEVEL = R.MAX_ONE_LEVEL   # of the drawn pixels (glraster.py, section 18)
MAX_BIG = R.MAX_BIG               # pixels more than one level off

CONTROLS = {1: "no dither", 2: "dither offset 0 instead of 0.5", 3: "lighting colour white", 4: "minification filter nearest",
            5: "texture env REPLACE", 6: "blending ignored", 7: "front faces culled", 8: "no specular term"}
CONTROL_STEPS = ("sandbox", "paths", "fuzz1_z0")


@pytest.fixture(scope="module")
def lib():
    return L.load()


@pytest.fixture(scope="module")
def setup():
    if not STEPS:
        pytest.skip("no terrain corpus under port/corpus/gl/c7 (python -m port.test.glcorpus export log/p5/gl/corpus/c7 c7)")
    return R.Setup(ROOT)


def _judge(lib, setup, step, mode=0):
    def patch(r, dll):
        if mode:
            t = f"#tune 11 {mode}\n".encode()
            dll.sg_raster_exec(r, t, len(t))
    post, got, st, unknown, pre = R.run(lib, setup, step, patch=patch)
    rep = R.report(post, got, pre)
    drawn = max(st[2], 1)
    return rep["diff"] - rep["gt1"], rep["gt1"], drawn, rep


@pytest.mark.parametrize("step", STEPS or ["-"])
def test_raster_frame_matches_the_original(lib, setup, step):
    one, big, drawn, rep = _judge(lib, setup, step)
    assert big <= MAX_BIG, f"{step}: {big} pixels more than one level off (max channel error {rep['max']})"
    assert one <= MAX_ONE_LEVEL * drawn, f"{step}: {one} one-level pixels of {drawn} drawn ({100.0 * one / drawn:.2f} %)"


RENDER_STEPS = [s for s in STEPS if G.meta(ROOT / "snaps" / s / "meta.txt").get("entry_kind") == G.ENTRY_RENDER]


@pytest.mark.parametrize("step", RENDER_STEPS or ["-"])
def test_chain_frame_matches_the_original(lib, setup, step):
    """The port's Terrain::render drives the rasteriser directly (no recorded stream between them): the frame still matches."""
    post, got, st, pre = R.run_chain(lib, setup, step)
    rep = R.report(post, got, pre)
    drawn = max(st[2], 1)
    one = rep["diff"] - rep["gt1"]
    assert rep["gt1"] <= MAX_BIG, f"{step}: {rep['gt1']} pixels more than one level off (max {rep['max']})"
    assert one <= MAX_ONE_LEVEL * drawn, f"{step}: {one} one-level pixels of {drawn} drawn"


INSTALL = G.ROOT / "original"


@pytest.mark.parametrize("step", RENDER_STEPS or ["-"])
def test_chain_from_the_ports_own_setup_matches_the_original(lib, setup, step):
    """No recorded state at all between the user's files and the pixels: the port's GL setup (texture load, lighting file, projection)
    and its Terrain::render drive the rasteriser; only the Terrain object and the arrays come from the snapshot."""
    if not (INSTALL / "Data" / "Textures" / "Parkland").is_dir():
        pytest.skip("no install with Data/Textures (original/)")
    post, got, st, pre = R.run_chain_from_setup(lib, setup, step, INSTALL)
    rep = R.report(post, got, pre)
    drawn = max(st[2], 1)
    one = rep["diff"] - rep["gt1"]
    assert rep["gt1"] <= MAX_BIG, f"{step}: {rep['gt1']} pixels more than one level off (max {rep['max']})"
    assert one <= MAX_ONE_LEVEL * drawn, f"{step}: {one} one-level pixels of {drawn} drawn"


@pytest.mark.parametrize("mode", sorted(CONTROLS))
def test_negative_control_breaks_the_criterion(lib, setup, mode):
    broken = []
    for step in CONTROL_STEPS:
        if step not in STEPS:
            continue
        one, big, drawn, rep = _judge(lib, setup, step, mode)
        if big > MAX_BIG or one > MAX_ONE_LEVEL * drawn:
            broken.append(step)
    assert broken, f"control {mode} ({CONTROLS[mode]}) passed the criterion on every pass: the corpus cannot see it"


def test_chain_control_the_lighting_file_reaches_the_pixels(lib, setup, tmp_path):
    """With the lighting file edited (ambient 200 -> 60) the setup-driven chain must no longer match the original's frame: the
    user's file really is what lights the picture."""
    import shutil
    if not (INSTALL / "Data" / "Textures" / "Parkland").is_dir():
        pytest.skip("no install with Data/Textures (original/)")
    dst = tmp_path / "install"
    (dst / "Data" / "Textures").mkdir(parents=True)
    shutil.copytree(INSTALL / "Data" / "Textures" / "Parkland", dst / "Data" / "Textures" / "Parkland")
    text = (INSTALL / "ParklandLighting.txt").read_text().replace("200 200 180", "60 60 60", 1)
    (dst / "ParklandLighting.txt").write_text(text)
    post, got, st, pre = R.run_chain_from_setup(lib, setup, "sandbox", dst)
    rep = R.report(post, got, pre)
    drawn = max(st[2], 1)
    one = rep["diff"] - rep["gt1"]
    assert rep["gt1"] > MAX_BIG or one > MAX_ONE_LEVEL * drawn


@pytest.mark.parametrize("step", STEPS or ["none"])
def test_fixed_point_setup_beats_the_linear_model(lib, setup, step):
    """Section 18: on every pass the generic GL's triangle setup (8-fractional-bit vertices of a viewport of half-size h - 2^-8, for
    coverage and attributes; the default) gives fewer one-level pixels than the earlier exact-edge, linear-sample-shift model (tune 26)."""
    if step == "none":
        pytest.skip("no corpus")
    one = []
    for knob in (0, 1):
        def patch(r, dll, knob=knob):
            if knob:
                dll.sg_raster_exec(r, b"#tune 26 1\n", 11)
        post, got, st, unknown, pre = R.run(lib, setup, step, patch=patch)
        rep = R.report(post, got, pre)
        one.append(rep["diff"] - rep["gt1"])
    assert one[0] < one[1], f"{step}: fixed-point setup {one[0]} one-level pixels, linear model {one[1]}"
