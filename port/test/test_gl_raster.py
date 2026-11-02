"""Phase 5 step 3 (re/PHASE5_TERRAIN_PLAN.md): the port's software rasteriser against the original's back-buffer frames.

Each corpus snapshot (re/tools/gl_corpus.py, exported with `port.test.glcorpus export`) holds one draw pass of the ORIGINAL
Terrain.dll code and the 800x600 RGB555 back buffer before and after it. The rasteriser replays the state stream and the
pass's GL calls (port/test/glraster.py) onto the same starting buffer; the result is compared with the original's pixel by pixel.

THE CRITERION. Not bit-exact, on purpose and measured: with the fixed-point triangle setup (section 18), the generic GL's specular
table and its tie rule (roadmap S3.1b, U-1300: log/roadmap/S3/1b_notes.md) the rasteriser writes the original's pixel in all but
0.06 % of the drawn pixels on average over the 49 passes of c7 and t3 (worst pass 0.21 %), every other pixel is one 5-bit level off
in one channel, and up to 13 pixels per pass differ by more (vertices and diagonal edges exactly on a pixel centre).
A snapshot passes when the one-level pixels are at most 1.5 % of the drawn pixels and at most 70 pixels differ by more. What the
pipeline reproduces exactly was fitted from the data and from probes against the real software GL (glprobe.py) and is written in
gl_raster.cpp and re/PHASE5_TERRAIN_PLAN.md: the GL_MODULATE lighting (the original's glTexEnvi call names GL_TEXTURE_2D, an
invalid target, so the mode stays modulate), the 4x4 ordered dither and the 31/255 channel scale (a flat-colour probe matches
exactly), glTexParameterf filters (GL_LINEAR, passed as floats) for the path textures, untextured fragments for an incomplete
texture, source-alpha blending, and the triangle setup: vertices scaled by 1 - 2^-8 / h about the viewport centre and rounded to
8 fractional bits, for coverage and attributes, sampled at pixel centres (section 18; `#tune 26 1` restores the linear model); the
specular term read from a 256-entry table over [0.0005^(1/shininess), 1] at the nearest entry (`#tune 27 1` restores the exact
pow); pixel centres exactly on an edge owned by edges pointing to +y, or to +x when horizontal (`#tune 10 2` restores -x);
since roadmap S3.21 (U-1392) the vertex positions by the generic GL's own float32 arithmetic (float32 matrices with the MVP updated
by every modelview operation, the translation added first, window = round-half-even(f32(ndc * (h - 2^-8)) + centre) to 2^-8;
`#tune 28 1` restores the double model) and triangles crossing the view volume clipped like the generic GL and drawn as fans
(`#tune 29 1` restores no clipping): the one-level pixels fell about tenfold (c7 sandbox 188 -> 11, zoom1 342 -> 11).

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


@pytest.mark.parametrize("step", STEPS or ["none"])
def test_specular_table_beats_the_exact_pow(lib, setup, step):
    """U-1300: on every pass the generic GL's specular table (256 entries over [0.0005^(1/shininess), 1], the nearest entry; the
    default) gives fewer one-level pixels than the exact (n.h)^shininess (tune 27)."""
    if step == "none":
        pytest.skip("no corpus")
    one = []
    for knob in (0, 1):
        def patch(r, dll, knob=knob):
            if knob:
                dll.sg_raster_exec(r, b"#tune 27 1\n", 11)
        post, got, st, unknown, pre = R.run(lib, setup, step, patch=patch)
        rep = R.report(post, got, pre)
        one.append(rep["diff"] - rep["gt1"])
    assert one[0] < one[1], f"{step}: specular table {one[0]} one-level pixels, exact pow {one[1]}"


# The two U-1300 rules, each judged where the earlier model fails the criterion: g6 (cov_b_paths with 60 % type-7 tiles, recorded
# in roadmap S3.1) draws 627 untextured lit tiles whose colour sits on a quantisation threshold of the specular term, and path bands
# whose horizontal bottom edge rounds to y = 344.5 exactly. Without the rule the pass fails G2; with it the pass meets it.
G6 = G.CORPUS / "g6"
U1300_CONTROLS = {"exact pow": ("i012_raised_strip", "#tune 27 1\n"), "tie rule -x": ("i005_raised_pathupd", "#tune 10 2\n")}


@pytest.mark.parametrize("name", sorted(U1300_CONTROLS))
def test_u1300_rule_is_needed_on_g6(lib, name):
    step, knob = U1300_CONTROLS[name]
    if not (G6 / "snaps" / step).is_dir():
        pytest.skip("no g6 corpus (python -m port.test.glcorpus export log/p5/gl/corpus/g6 g6 1,3,4)")
    st6 = R.Setup(G6)
    res = []
    for k in ("", knob):
        def patch(r, dll, k=k):
            if k:
                dll.sg_raster_exec(r, k.encode(), len(k))
        post, got, st, unknown, pre = R.run(lib, st6, step, patch=patch)
        rep = R.report(post, got, pre)
        one, big, drawn = rep["diff"] - rep["gt1"], rep["gt1"], max(st[2], 1)
        res.append(big <= MAX_BIG and one <= MAX_ONE_LEVEL * drawn)
    assert res == [True, False], f"{name} on g6/{step}: default meets G2 {res[0]}, control meets G2 {res[1]}"


def _probe_or_skip():
    from port.test import glprobe as GP
    if not GP.REPLAY.exists():
        pytest.skip("no re/tools/glexact/glreplay.exe (build.bat): the real software GL is the reference")
    return GP


def test_probe_lit_untextured_quads_are_exact(lib):
    """Probe (glprobe untex lit): 1,452 constant-colour quads lit by the terrain's light with random normals, through the real GL
    and GlRaster: every pixel equal with the specular table, not with the exact pow."""
    GP = _probe_or_skip()
    sc, _ = GP.untex_scene("lit")
    d = sc.write("untex_lit")
    real = GP.real_frame(d)
    assert int((GP.port_frame(lib, d) != real).sum()) == 0
    assert int((GP.port_frame(lib, d, {27: 1}) != real).sum()) > 0


def test_probe_tie_rule(lib):
    """Probe (glprobe tie): triangles with vertices on pixel centres. The default rule leaves the fewest coverage differences of the
    six rules of tune 10 (48 measured, all at vertices or diagonal edges exactly on a centre, U-1391)."""
    GP = _probe_or_skip()
    d = GP.tie_scene().write("tie")
    real = GP.real_frame(d)
    cov = {t: int(((GP.port_frame(lib, d, {10: t}) != 0) != (real != 0)).sum()) for t in range(6)}
    assert cov[0] <= 48 and all(cov[0] < cov[t] for t in range(1, 6)), cov


# Roadmap S3.10 (U-1640, log/roadmap/S3/10_notes.md): GlRaster's fast pixel loop (exact integer edge functions and row spans, the
# checked reciprocal path, pixel-loop workers) must give the general loop's frame bit for bit, so every G2 verdict above stays what
# it was. Each snapshot of c7 and g6 is replayed with the general loop (`#fastpath 0`, the rasteriser before S3.10) and with the
# default fast loop, its exact branch alone (`#fastpath 2`) and 4 workers (`#threads 4`): frames and pixel counters must be equal.
# log/roadmap/S3/10_equiv.py runs the same over every recorded corpus (c3 .. c7, g2 .. g6: 268 passes, 0 differ).
FAST_LABELS = ("c7", "g6")
FAST_MODES = {"fast": "", "exact": "#fastpath 2\n", "threads": "#threads 4\n"}
# Negative control: reciprocals rounded to float (`#fastpath 4`, errors above what the boundary checks allow) changes pixels; on c7
# it does on 20 of 34 passes, these two by 5 pixels each. Blind by measurement: `#fastpath 3` (no boundary checks at all) leaves all
# 268 recorded passes unchanged, since no pixel of them comes within the double quotients' error of a rounding boundary.
CRUDE_STEPS = ("fuzz8_z1", "fuzz9_z1")


def _frame(lib, setup, step, line):
    def patch(r, dll):
        if line:
            dll.sg_raster_exec(r, line.encode(), len(line))
    post, got, st, unknown, pre = R.run(lib, setup, step, patch=patch)
    return got.copy(), st


@pytest.mark.parametrize("label", FAST_LABELS)
def test_fast_loop_equals_the_general_loop(lib, label):
    root = G.CORPUS / label
    steps = G.snaps(root)
    if not steps:
        pytest.skip(f"no corpus port/corpus/gl/{label}")
    setup_ = R.Setup(root)
    bad = []
    for step in steps:
        ref, rst = _frame(lib, setup_, step, "#fastpath 0\n")
        for mode, line in FAST_MODES.items():
            got, st = _frame(lib, setup_, step, line)
            if int((got != ref).sum()) or st[:5] != rst[:5]:
                bad.append((step, mode, int((got != ref).sum()), st[:5], rst[:5]))
    assert not bad, bad[:5]


@pytest.mark.parametrize("step", RENDER_STEPS or ["-"])
def test_fast_loop_with_workers_equals_the_general_loop_on_the_chain(lib, setup, step):
    """The port's Terrain::render drives the rasteriser directly: 4 workers give the general loop's frame and counters."""
    if step == "-":
        pytest.skip("no corpus")
    out = []
    for line in (b"#fastpath 0\n", b"#threads 4\n"):
        def patch(r, dll, line=line):
            dll.sg_raster_exec(r, line, len(line))
        post, got, st, pre = R.run_chain(lib, setup, step, patch=patch)
        out.append((got.copy(), st[:5]))
    assert int((out[0][0] != out[1][0]).sum()) == 0 and out[0][1] == out[1][1], step


def test_fast_loop_control_float_reciprocals_changes_the_frame(lib, setup):
    changed = []
    for step in CRUDE_STEPS:
        if step not in STEPS:
            continue
        ref, _ = _frame(lib, setup, step, "#fastpath 0\n")
        got, _ = _frame(lib, setup, step, "#fastpath 4\n")
        if int((got != ref).sum()):
            changed.append(step)
    assert changed, "control #fastpath 4 (float reciprocals) left every frame unchanged: the comparison cannot see it"


# ---- roadmap S3.21 (U-1392): the generic GL's float32 vertex arithmetic and its frustum clipping, measured against the real
# software GL's GL_FEEDBACK output (log/roadmap/S3/21_notes.md: every vertex of the 268 corpus passes, 973,749, equal; the clipper's
# output triangles equal on the passes checked). The G2 criterion above is too loose to see them, so these tests bound the one-level
# pixels of four c7 passes tightly: the rules meet the bound, and each old behaviour breaks it (the negative controls:
# `#tune 28 1` the earlier double vertex model, `#tune 29 1` no clipping).
S321_STEPS = ("sandbox", "zoom1", "z2_r1", "hole")
S321_MAX_ONE_LEVEL_SUM = 80   # measured 40 (11 + 11 + 11 + 7); tune 28 = 1 gives 1,213, tune 29 = 1 216


def _one_level(lib, setup, step, lines=""):
    def patch(r, dll):
        if lines:
            t = lines.encode()
            dll.sg_raster_exec(r, t, len(t))
    post, got, st, unknown, pre = R.run(lib, setup, step, patch=patch)
    rep = R.report(post, got, pre)
    return rep["diff"] - rep["gt1"]


def _s321_sum(lib, setup, lines):
    steps = [s for s in S321_STEPS if s in STEPS]
    if len(steps) != len(S321_STEPS):
        pytest.skip("c7 lacks one of " + ", ".join(S321_STEPS))
    return {s: _one_level(lib, setup, s, lines) for s in steps}


def test_s321_rules_meet_the_tight_bound(lib, setup):
    per = _s321_sum(lib, setup, "")
    assert sum(per.values()) <= S321_MAX_ONE_LEVEL_SUM, per


@pytest.mark.parametrize("control", ["#tune 28 1\n", "#tune 29 1\n"])
def test_s321_old_behaviour_breaks_the_tight_bound(lib, setup, control):
    per = _s321_sum(lib, setup, control)
    assert sum(per.values()) > S321_MAX_ONE_LEVEL_SUM, (control, per)


@pytest.mark.parametrize("step", STEPS or ["none"])
def test_s321_vertex_rule_is_no_worse_than_the_double_model(lib, setup, step):
    if step == "none":
        pytest.skip("no corpus")
    assert _one_level(lib, setup, step) <= _one_level(lib, setup, step, "#tune 28 1\n"), step


# ---- roadmap S3.21b (U-1309): the generic GL's incremental 16.16 attribute stepping, opt-in (`#tune 30`), measured on probes of the
# real GL (log/roadmap/S3/21_ramp.py dense, 21_modprobe.py; the real frames are the probe scenes' dib_post.bin, local corpus).
# Untextured: exact on the slow ramp (705 -> 0 px); textured (MODULATE) triangles are not covered by the model (constant texture x
# Gouraud colour 15 -> 298 with the 5-bit quantum), so the default stays off and `#tune 30 2` steps untextured triangles only.
S321B_PROBE = G.CORPUS / "probe" / "s321_ramp_dense"


def _probe_diff(lib, d, knobs):
    import numpy as np
    from port.test import glprobe as GP
    real = np.frombuffer((d / "snaps" / "s1" / "dib_post.bin").read_bytes(), "<u2")
    port = GP.port_frame(lib, d, knobs)
    return int((real != port).sum())


def test_s321b_stepping_is_exact_on_the_ramp_probe(lib):
    if not (S321B_PROBE / "snaps" / "s1" / "dib_post.bin").exists():
        pytest.skip("no s321_ramp_dense probe (run log/roadmap/S3/21_ramp.py dense with re/tools/glexact/glreplay.exe)")
    assert _probe_diff(lib, S321B_PROBE, {30: 2}) == 0


def test_s321b_control_stepping_off_reads_red(lib):
    if not (S321B_PROBE / "snaps" / "s1" / "dib_post.bin").exists():
        pytest.skip("no s321_ramp_dense probe")
    assert _probe_diff(lib, S321B_PROBE, None) > 100   # measured 705 with the plane


# ---- roadmap S3.29 (U-1309): the textured walk, opt-in (`#tune 32 1`), measured on probes of the real GL (log/roadmap/S3/29_probes.py
# through re/tools/glexact/glreplay.exe; local corpus port/corpus/gl/probe/s329_*, the S3.21 ones s321_*): a textured triangle whose
# texture has a GL_LINEAR filter is interpolated in float32 (float32 gradients from the lowest vertex, rows walked from the largest y,
# float32 sums along each span) and its unblended GL_MODULATE pixels are filtered, modulated and dithered in float32. Exact on the
# probes below; NOT exact on every probe (the S3.29_LEFT bounds: 1-2 float32 ulps of u or v the measured forms do not reproduce), so
# the default stays off. Controls: the plane (rule off), the double final stage (`#tune 35 1`), the highest-vertex anchor
# (`#tune 33 1`), the stream-first anchor (`#tune 33 2`), no restart at the middle vertex (`#tune 34 1`).
S329_EXACT = ("s321_gour_rand", "s321_gour_const", "s329_cramp_lin", "s329_cramp_mix", "s329_uramp_lin", "s329_uramp_mix",
              "s329_vramp_lin", "s329_fin_near", "s329_fin_lin")
S329_LEFT = {"s329_perm_rand": 19, "s329_perm_big": 13, "s321_const_rand": 5, "s329_alt_big": 1}   # measured with the rule (every
# differing pixel: perm_rand 16 values + 3 coverage, perm_big 12 + 1; the coverage ones are the same with the rule off)
S329_PLANE = {"s329_alt_big": 64599, "s321_const_rand": 285, "s321_gour_rand": 306, "s329_perm_big": 935, "s329_fin_near": 10064,
              "s329_fin_lin": 9792}   # measured with the rule off
S329_CONTROLS = [({32: 1, 35: 1}, "s329_fin_near", 1000), ({32: 1, 35: 1}, "s329_fin_lin", 1000), ({32: 1, 33: 1}, "s329_alt_big", 1000),
                 ({32: 1, 33: 2}, "s329_perm_big", 30), ({32: 1, 34: 1}, "s329_perm_big", 20)]


def _s329_probe(name):
    d = G.CORPUS / "probe" / name
    if not (d / "snaps" / "s1" / "dib_post.bin").exists():
        pytest.skip(f"no {name} probe (run log/roadmap/S3/29_probes.py with re/tools/glexact/glreplay.exe)")
    return d


@pytest.mark.parametrize("name", S329_EXACT)
def test_s329_walk_is_exact_on_the_probe(lib, name):
    assert _probe_diff(lib, _s329_probe(name), {32: 1}) == 0


@pytest.mark.parametrize("name", sorted(S329_LEFT))
def test_s329_walk_residue_is_bounded(lib, name):
    assert _probe_diff(lib, _s329_probe(name), {32: 1}) <= S329_LEFT[name]


@pytest.mark.parametrize("name", sorted(S329_PLANE))
def test_s329_control_rule_off_reads_red(lib, name):
    n = _probe_diff(lib, _s329_probe(name), None)
    assert n > 10 * max(1, S329_LEFT.get(name, 0)), (name, n)


@pytest.mark.parametrize("knobs,name,floor", S329_CONTROLS)
def test_s329_control_wrong_detail_reads_red(lib, knobs, name, floor):
    n = _probe_diff(lib, _s329_probe(name), knobs)
    assert n > floor, (knobs, name, n)


@pytest.mark.parametrize("step", ["sandbox", "fuzz1_z0", "fuzz7_z1"])
def test_s329_walk_is_the_same_in_every_pixel_loop(lib, setup, step):
    """the general loop, the fast loop and 4 workers give the same frame with the walk on (S3.10's identity, tune 32)"""
    if step not in STEPS:
        pytest.skip("c7 lacks " + step)
    frames = [_frame(lib, setup, step, "#tune 32 1\n" + extra)[0] for extra in ("#fastpath 0\n", "", "#fastpath 2\n", "#threads 4\n")]
    for f in frames[1:]:
        assert (f == frames[0]).all(), step


def test_s329_walk_lowers_the_c7_one_level_pixels(lib, setup):
    steps = [s for s in ("sandbox", "zoom1", "z2_r1", "hole", "fuzz1_z0", "fuzz7_z1", "view3") if s in STEPS]
    if len(steps) != 7:
        pytest.skip("c7 lacks one of the seven passes")
    on = sum(_one_level(lib, setup, s, "#tune 32 1\n") for s in steps)
    off = sum(_one_level(lib, setup, s) for s in steps)
    assert on <= 154 and off >= 173, (on, off)   # measured 154 / 173
