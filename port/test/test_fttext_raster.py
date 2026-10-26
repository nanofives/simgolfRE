"""Roadmap step S5.3: the FreeType raster of jgld's TextOutA (port/src/sim/fttext_raster.cpp, FtTextMetrics::draw_glyphs in
port/src/sim/fttext.cpp; notes log/roadmap/S5/3_notes.md).

Evidence:
  * BLEND: the C++ blend (test entry fttext_raster.blend5) equals the host GDI's ClearType store on the 16-bit DIB for every grey text
    5-bit value x surface 5-bit value x level 0..6: the GDI provider draws one text white on black (its 5-bit channel values
    name the level, 7 distinct) and then in each of the 1,024 grey pairs, and every channel of every pixel is compared.
  * CORPUS: the FreeType provider's raster of S5.2's label corpus is deterministic, equal on x64 and Win32, and its digest
    and per-font pixel differences against the GDI reference are the measured ones (MEASURED below; the raster is a model
    of GDI ClearType, not equal to it: 3_notes.md section 6 gives the causes).
  * CHECKPOINTS: the replayed label calls of the six PASS gfx checkpoints give the measured pixel differences.
  * CONTROLS (g_negctl_fttext 5 glyph origin + 1 px, 6 colour one 5-bit red step, 7 the raster's hinting off): each changes
    the raster and raises the difference to GDI on a corpus subset.
Tests skip when the library has no fttext / textlab / gditext module, FreeType is absent, the corpora are absent, or the host
GDI does not render ClearType at contrast 1200 (the measured host, labels.py facts).
"""
from __future__ import annotations

import json
import os

import numpy as np
import pytest

from port.test import labels as LB
from port.test import lib as L

pytestmark = pytest.mark.port

# measured on LIVING-PC (2026-10-24) with the host's arial.ttf and the game's manu3_.TTF / KLEPTO__.TTF
DIGEST = "5a336eba875e1d94d47aa74f98302a4eb67578f38b874ec0032e7dbb2aafa5fd"
MEASURED_PX = {"0x00519928": 391631, "0x00519948": 22359, "0x00519a40": 108411, "0x00519fd8": 176256,
               "0x0051a028": 125525, "0x0051b360": 7339388, "0x00821020": 200488, "0x00821ee8": 282308,
               "0x00821f28": 90227}
MEASURED_CHECKPOINTS = {"sandbox_basic/menu": 3369, "cov_b_location/refused": 20138, "cov_b_location/site_hover": 20138,
                        "cov_b_location/buttons": 20138, "cov_b_location/tip": 20252, "cov_b_location/back": 20780}
CONTROLS = [(5, "glyph origin + 1 px"), (6, "colour one 5-bit red step"), (7, "raster hinting off")]
BLEND_TEXT = b"Hello World 0123 Wavy MNOPQ"
BLEND_FONT = 0x00519A40   # Klepto ITC TT 24: wide strokes, every level occurs in every channel


def _need_corpus():
    for p in (LB.OUT / "corpus.json", LB.OUT / "rasters" / "gdi_x64.npz",
              LB.OUT / "checkpoints" / "sandbox_basic.x64.calls.bin", LB.OUT / "checkpoints" / "cov_b_location.x64.calls.bin"):
        if not p.exists():
            pytest.skip(f"no {p.relative_to(LB.ROOT)} (labels.py corpus / reference / checkpoints)")


def _need_host():
    if os.name != "nt":
        pytest.skip("no host GDI")
    h = LB.host_smoothing()
    if h.get("SPI_GETFONTSMOOTHINGTYPE") != 2 or h.get("SPI_GETFONTSMOOTHINGCONTRAST") != 1200 or \
            h.get("SPI_GETFONTSMOOTHINGORIENTATION") != 1:
        pytest.skip(f"the host GDI is not ClearType RGB at contrast 1200: {h}")


@pytest.fixture(scope="module")
def ftlib():
    lib = L.load()
    for f in ("fttext.attach", "fttext_raster.blend5", "textlab.batch", "gditext.attach"):
        if not lib.has(f):
            pytest.skip(f"the library has no {f}")
    st = lib.state()
    try:
        if not st.call("fttext.available", []).eax:
            pytest.skip("built without FreeType")
    finally:
        st.close()
    return lib


@pytest.fixture(scope="module")
def corpus():
    _need_corpus()
    return LB.load_corpus()


@pytest.fixture(scope="module")
def ft_raster(ftlib, corpus):
    return LB.render_corpus(ftlib, "freetype")


def _grey(v5: int) -> int:
    return 0x80000000 | (v5 << 10) | (v5 << 5) | v5


def test_blend_equals_gdi(ftlib, corpus):
    _need_host()
    st = ftlib.state()
    try:
        table = np.array([[[st.call("fttext_raster.blend5", [f, b, a, 0]).eax for b in range(32)] for f in range(32)]
                          for a in range(7)])
    finally:
        st.close()
    white = table[:, 31, 0]
    assert len(set(white.tolist())) == 7, "white on black must name the 7 levels"
    lvl_of = {int(v): a for a, v in enumerate(white)}
    w, h, x, y = 400, 40, 4, 30
    lab = LB.Lab(ftlib, "gdi", LB.start_image(corpus["image"]))
    try:
        jobs = [(BLEND_FONT, _grey(31), 0, x, y, w, h, BLEND_TEXT, None, 0, 1)]
        jobs += [(BLEND_FONT, _grey(f), (b << 10) | (b << 5) | b, x, y, w, h, BLEND_TEXT, None, 0, 1)
                 for f in range(32) for b in range(32)]
        out = lab.render(jobs)
    finally:
        lab.close()
    ref = np.stack([(out[0] >> k) & 31 for k in (10, 5, 0)], -1)
    levels = np.vectorize(lvl_of.get)(ref)
    for a in range(7):
        assert (levels == a).any(), f"level {a} does not occur in the probe text"
    bad = 0
    for i, (f, b) in enumerate((f, b) for f in range(32) for b in range(32)):
        got = np.stack([(out[i + 1] >> k) & 31 for k in (10, 5, 0)], -1)
        bad += int((got != table[levels, f, b]).sum())
    assert bad == 0, f"{bad} channel values differ from the blend model"


def test_corpus_raster_measured(ftlib, corpus, ft_raster):
    s = LB.reference_summary(corpus, ft_raster)
    assert s["digest_all"] == DIGEST
    ref = LB.load_rasters(LB.OUT / "rasters" / "gdi_x64.npz")
    res = LB.compare_corpus(ft_raster, ref, corpus)
    assert {f: v["px_differ"] for f, v in res["per_font"].items()} == MEASURED_PX


def test_corpus_raster_deterministic(ftlib, corpus, ft_raster):
    again = LB.render_corpus(ftlib, "freetype")
    assert LB.reference_summary(corpus, again)["digest_all"] == LB.reference_summary(corpus, ft_raster)["digest_all"]


def test_corpus_raster_win32_equals_x64(corpus, ft_raster):
    try:
        lib32 = LB.load_lib("win32", os.environ.get("SG_PORTSIM_W32_DIR"))
    except Exception as e:   # RunnerUnavailable or a missing private build
        pytest.skip(f"Win32 backend unavailable: {e}")
    if not lib32.has("fttext.attach"):
        pytest.skip("the Win32 library has no fttext module")
    r32 = LB.render_corpus(lib32, "freetype")
    assert LB.reference_summary(corpus, r32)["digest_all"] == LB.reference_summary(corpus, ft_raster)["digest_all"]


def test_checkpoints_measured(ftlib, corpus):
    res = LB.compare_checkpoints(ftlib, "freetype", LB.start_image(corpus["image"]))
    assert {k: v["px_vs_gdi"] for k, v in res.items()} == MEASURED_CHECKPOINTS


def test_gdi_provider_unchanged(ftlib, corpus):
    r = LB.render_corpus(ftlib, "gdi")
    ref = LB.load_rasters(LB.OUT / "rasters" / "gdi_x64.npz")
    assert LB.compare_corpus(r, ref, corpus)["px_differ"] == 0


@pytest.mark.parametrize("mode,what", CONTROLS)
def test_negctl_red(ftlib, corpus, ft_raster, mode, what):
    ents = corpus["entries"]
    # a subset with every font and configuration: every 23rd job
    idx = list(range(0, len(corpus["jobs"]), 23))
    jobs = LB.corpus_jobs(corpus)
    ref = LB.load_rasters(LB.OUT / "rasters" / "gdi_x64.npz")
    prev = ftlib.negctl("fttext.fontWidth", mode)
    lab = LB.Lab(ftlib, "freetype", LB.start_image(corpus["image"]))
    try:
        got = lab.render([jobs[i] for i in idx])
    finally:
        lab.close()
        ftlib.negctl("fttext.fontWidth", prev or 0)
    changed = sum(1 for i, g in zip(idx, got) if (g != ft_raster[i]).any())
    base_px = sum(int((ft_raster[i] != ref[i]).sum()) for i in idx)
    ctl_px = sum(int((g != ref[i]).sum()) for i, g in zip(idx, got))
    fonts = {ents[corpus["jobs"][i][1]]["fontref"] for i, g in zip(idx, got) if (g != ft_raster[i]).any()}
    assert changed > len(idx) // 2, f"{what}: only {changed} of {len(idx)} jobs changed"
    assert len(fonts) == len(MEASURED_PX), f"{what}: not every font reads it"
    assert ctl_px > base_px, f"{what}: the difference to GDI did not grow ({ctl_px} <= {base_px})"

