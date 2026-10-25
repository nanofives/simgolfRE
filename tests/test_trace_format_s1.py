"""S1.1 (a), U-1067: validate() rejects an input-class MSG record that no harness DELIVER explains.

Static. Synthetic traces prove the rule (the cov_r_quit second-recording shape: KEYDOWN VK 174 with no DELIVER); the stored
traces (port/corpus, local, gitignored) prove it does not reject what was accepted: all 266 validate, measured 2026-10-24.
"""
import sys

import pytest

from conftest import ROOT

pytestmark = pytest.mark.static

sys.path.insert(0, str(ROOT / "re" / "tools"))
sys.path.insert(0, str(ROOT / "tests"))
import trace_format as TF  # noqa: E402
from test_trace_format import META, build  # noqa: E402


def parsed(extra):
    return TF.parse(build(extra=extra))


def test_cov_r_quit_shape_is_rejected():
    """KEYDOWN VK_VOLUME_DOWN (174), then its WM_CHAR-free onKey, with no DELIVER anywhere before it."""
    def extra(w):
        w.raw(TF.MSG, 0, "<III", 0x100, 174, 1)
        w.raw(TF.HANDLER, 7, "<II", 174, 174)
    errs = TF.validate(parsed(extra))
    assert any("0x0100 wp=174" in e and "no preceding harness DELIVER" in e for e in errs), errs


def test_baseline_synthetic_trace_still_valid():
    assert TF.validate(TF.parse(build())) == []


def test_delivery_explains_exactly_one_message():
    def extra(w):
        w.deliver("post", 1, 0, 0x100, 0x41, 1)
        w.raw(TF.MSG, 0, "<III", 0x100, 0x41, 1)
        w.raw(TF.MSG, 0, "<III", 0x100, 0x41, 1)      # a second one with no second DELIVER
    errs = TF.validate(parsed(extra))
    assert len([e for e in errs if "no preceding harness DELIVER" in e]) == 1, errs


def test_derived_messages_need_no_delivery():
    def extra(w):
        w.raw(TF.MSG, 0, "<III", 0x102, 0x41, 1)      # WM_CHAR made by TranslateMessage
        w.raw(TF.MSG, 0, "<III", 0x200, 0, 0x1e0190)  # WM_MOUSEMOVE
    assert TF.validate(parsed(extra)) == []


def test_mouse_button_without_delivery_is_rejected():
    def extra(w):
        w.raw(TF.MSG, 0, "<III", 0x201, 1, 0x1e0190)
    assert any("0x0201" in e for e in TF.validate(parsed(extra)))


def test_slice_exempts_only_the_first_frame():
    """A segment slice can start with a message already in the Windows queue (segment_corpus.md, INPUT IN FLIGHT)."""
    meta = {**META, "slice": {"scenario": "synthetic"}}
    for late, ok in ((False, True), (True, False)):
        w = TF.Writer(meta)
        w.frame()
        w.raw(TF.MSG, 0, "<III", 0x100, 38, 1) if not late else None
        w.frame()
        w.raw(TF.MSG, 0, "<III", 0x100, 38, 1) if late else None
        w.poll(final=True)
        errs = TF.validate(TF.parse(w.end()))
        assert (not any("DELIVER" in e for e in errs)) == ok, errs


def stored():
    return sorted((ROOT / "port" / "corpus").glob("traces/*.trace")) + \
        sorted((ROOT / "port" / "corpus").glob("segments/*/*/trace.trace"))


def test_every_stored_trace_still_validates():
    files = stored()
    if not files:
        pytest.skip("port/corpus is local and not present")
    bad = {}
    for p in files:
        errs = [e for e in TF.validate(TF.read(p)) if "no preceding harness DELIVER" in e]
        if errs:
            bad[str(p.relative_to(ROOT))] = errs[:2]
    assert not bad, bad
