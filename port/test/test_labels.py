"""Roadmap step S5.2: the label pixel corpus, module textlab and the GDI reference raster (port/test/labels.py; notes
log/roadmap/S5/2_notes.md).

Evidence:
  * REFERENCE: the GDI provider's raster of the whole corpus (port/corpus/labels/corpus.json, built by `labels.py corpus`
    from the widths recordings) equals the digests of port/corpus/labels/reference.json (`labels.py reference`: two
    runs on x64 and two on Win32, all equal), on x64 and on Win32.
  * CHECKPOINTS: every label call of the six gfx checkpoints that PASS at HEAD (recorded by `labels.py checkpoints` with
    textlab.recordStart over a real boot) replayed on its recorded pre-call box gives the GDI run's post-call words, and,
    for the calls whose box the frame still shows, the original's frame words.
  * CONTROLS (g_negctl_textlab): the last glyph one pixel right, the colour one RGB555 step off, the last character
    dropped, the run one pixel right: each RED on the corpus and on every checkpoint.
Tests skip when the library has no textlab module or the local corpora are absent.
"""
from __future__ import annotations

import json
import os
import pathlib

import numpy as np
import pytest

from port.test import labels as LB
from port.test import lib as L

pytestmark = pytest.mark.port
NEGCTL = [(1, "the last glyph of every run one pixel right"), (2, "the text colour one RGB555 step off"),
          (3, "the last character dropped"), (4, "the whole run one pixel right")]


def _need_corpus():
    for p in (LB.OUT / "corpus.json", LB.OUT / "reference.json", LB.OUT / "rasters" / "gdi_x64.npz",
              LB.OUT / "checkpoints" / "sandbox_basic.x64.calls.bin",
              LB.OUT / "checkpoints" / "cov_b_location.x64.calls.bin"):
        if not p.exists():
            pytest.skip(f"no {p.relative_to(LB.ROOT)} (labels.py corpus / reference / checkpoints)")


@pytest.fixture(scope="module")
def lab_lib():
    lib = L.load()
    if not lib.has("textlab.draw") or not lib.has("gditext.attach"):
        pytest.skip("the library has no textlab / gditext module")
    if os.name != "nt":
        pytest.skip("no host GDI")
    return lib


@pytest.fixture(scope="module")
def lab_lib32():
    d = os.environ.get("SG_PORTSIM_W32_DIR")
    try:
        lib = LB.load_lib("win32", d)
    except Exception as e:   # RunnerUnavailable or a missing private build
        pytest.skip(f"Win32 backend unavailable: {e}")
    if not lib.has("textlab.draw"):
        pytest.skip("the Win32 library has no textlab module")
    return lib


@pytest.fixture(scope="module")
def corpus():
    _need_corpus()
    return LB.load_corpus()


@pytest.fixture(scope="module")
def reference():
    _need_corpus()
    return json.loads((LB.OUT / "reference.json").read_text())


# ---------------------------------------------------------------------------------------------------- static
@pytest.mark.static
def test_colour_conversion_is_the_run_drawers():
    # white 0x80007fff -> 0xf8 per channel; one bit per channel (SetRunColour's sar 7 / sar 2 / shl 3, 5-5-5 split)
    assert LB.colorref(0x80007FFF) == 0xF8F8F8
    assert LB.colorref(0x80000000) == 0
    assert LB.colorref(0x80004210) == 0x808080
    assert LB.colorref(0x80007C00) == 0x0000F8
    assert LB.word555(LB.colorref(0x80007FFF)) == 0x7FFF


@pytest.mark.static
def test_corpus_definition(corpus):
    ents = corpus["entries"]
    assert len(ents) == sum(corpus["per_font"].values())
    keys = {(e["fontref"], e["text"]) for e in ents}
    assert len(keys) == len(ents)
    assert all("\t" not in bytes.fromhex(e["text"]).decode("latin-1") for e in ents)
    assert all(b"_" not in bytes.fromhex(e["text"]) for e in ents)
    cfg = {j[0] for j in corpus["jobs"]}
    assert cfg == {"base", "inverse", "sweep", "shadow"}
    assert len(corpus["colours"]) == len(LB.scan_colours()) >= 20
    for e in ents:
        f = corpus["fonts"][f"0x{e['fontref']:08x}"]
        assert e["h"] == f["height"] + 8 and e["y"] == 4 + f["ascent"]


@pytest.mark.static
def test_corpus_is_rebuilt_identically(corpus):
    again = LB.build_corpus()
    assert json.loads(json.dumps(again)) == corpus


# ---------------------------------------------------------------------------------------------------- the entries
def test_no_provider_is_a_missing_resource(lab_lib):
    st = lab_lib.state()
    try:
        surf = st.map(LB.OUT_VA, (L.C.c_uint8 * 64)())
        text = st.map(LB.TEXT_VA, (L.C.c_uint8 * 16)(*b"A"))
        with pytest.raises(L.SgError) as e:
            st.call("textlab.draw", [0x0051A028, LB.TEXT_VA, 1, 2, 10, 0xFFFFFF, LB.OUT_VA, 4, 8])
        assert e.value.code == L.SG_E_NO_RESOURCE
        del surf, text
    finally:
        st.close()


def test_draw_on_the_address_space_equals_the_batch(lab_lib, corpus):
    """textlab.draw (one surface in the address space) and textlab.batch give the same words for the same job"""
    ref = LB.load_rasters(LB.OUT / "rasters" / "gdi_x64.npz")
    lab = LB.Lab(lab_lib, "gdi", LB.start_image(corpus["image"]))
    try:
        for k in (0, 1, len(corpus["entries"]) // 2, len(corpus["entries"]) - 1):
            e = corpus["entries"][k]
            t = bytes.fromhex(e["text"])
            surf = np.zeros((e["h"], e["w"]), "<u2")
            L.C.memmove(lab.out, surf.tobytes(), surf.nbytes)
            L.C.memmove(lab.text, t, len(t))
            got = lab.st.call("textlab.draw", [e["fontref"], LB.TEXT_VA, len(t), e["x"], e["y"],
                                               LB.colorref(LB.WHITE), LB.OUT_VA, e["w"], e["h"]]).eax
            assert got == 1
            words = np.frombuffer(bytes(lab.out)[:surf.nbytes], "<u2").reshape(surf.shape)
            assert np.array_equal(words, ref[k]), k   # jobs 0..n-1 are the base configuration, in entry order
    finally:
        lab.close()


def test_width_and_ascent_entries(lab_lib, corpus):
    lab = LB.Lab(lab_lib, "gdi", LB.start_image(corpus["image"]))
    try:
        e = corpus["entries"][0]
        t = bytes.fromhex(e["text"])
        L.C.memmove(lab.text, t, len(t))
        assert lab.st.call("textlab.width", [e["fontref"], LB.TEXT_VA, len(t)]).eax == e["width"]
        f = corpus["fonts"][f"0x{e['fontref']:08x}"]
        assert lab.st.call("textlab.ascent", [e["fontref"]]).eax == f["w10"]
    finally:
        lab.close()


# ---------------------------------------------------------------------------------------------------- reference
def test_reference_is_deterministic_and_equal_across_backends(reference):
    assert reference["deterministic"] == {"x64": True, "win32": True}
    assert reference["x64_equals_win32"]
    assert sum(reference["per_font_jobs"].values()) == reference["jobs"]


def test_gdi_raster_equals_the_reference_x64(lab_lib, corpus, reference):
    r = LB.render_corpus(lab_lib, "gdi")
    s = LB.reference_summary(corpus, r)
    assert s["digest_all"] == reference["digest_all"]


def test_gdi_raster_equals_the_reference_win32(lab_lib32, corpus, reference):
    r = LB.render_corpus(lab_lib32, "gdi")
    s = LB.reference_summary(corpus, r)
    assert s["digest_all"] == reference["digest_all"]


def test_checkpoint_labels_replay_exactly(lab_lib, corpus):
    res = LB.compare_checkpoints(lab_lib, "gdi", LB.start_image(corpus["image"]))
    assert len(res) == 6
    for k, v in res.items():
        assert v["calls"] > 0 and v["on_frame"] > 0, k
        assert v["px_vs_gdi"] == 0 and v["px_vs_original"] == 0, (k, v)


# ---------------------------------------------------------------------------------------------------- controls
@pytest.mark.parametrize("mode,what", NEGCTL)
def test_negctl_red(lab_lib, corpus, mode, what):
    ref = LB.load_rasters(LB.OUT / "rasters" / "gdi_x64.npz")
    r = LB.render_corpus(lab_lib, "gdi", negctl=mode)
    res = LB.compare_corpus(r, ref, corpus)
    assert res["px_differ"] > 0, what
    prev = lab_lib.negctl("textlab.draw", mode)
    try:
        cp = LB.compare_checkpoints(lab_lib, "gdi", LB.start_image(corpus["image"]))
    finally:
        lab_lib.negctl("textlab.draw", prev)
    assert all(v["px_vs_gdi"] > 0 and v["px_vs_original"] > 0 for v in cp.values()), (what, cp)
