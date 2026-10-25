"""Roadmap S1.3 (log/roadmap/S1/3_notes.md, U-1066 parts 2 and 3): the two scenarios cov_r_theme_cancel and cov_r_palette_hot
(re/tools/scenario_s1_3.py) run from process start by the port, every record equal to the original's, and what they judge.

cov_r_theme_cancel: showThemePackSelect's Cancel (hot button 1). listThemes sets g_theme to "Standard" on entry, so Cancel is
visible only after a theme row was pressed in the same visit: the scenario's third visit does that, then Start New Game ->
Impossible -> Northeast reads g_theme. Control showThemePackSelect 2 (Cancel keeps the theme) reads RED at record 0.

cov_r_palette_hot: the Build Course palette's two fixed hotspots, hit-test codes 0x15 (Undo) and 0x16 (the wrench tab), which
showInfoCursorPanel draws with the palette SpriteRefs 84 and 88, the two cuts loadInterfaceArt writes. Those cut fields are read
by drawing only (hitTestSlot432f90 walks slots 0..15, SpriteRefs 0..60), so no sim hash holds them: control loadInterfaceArt 3
(the two cuts skipped) reads GREEN in the boot run on this scenario, as on the 60 older ones. The cut fields are compared as
state against the original's frame-head images of the capture (`<checkpoint>.fh.bin`) at the checkpoints after each hotspot."""
from __future__ import annotations

import json
import struct

import pytest

from port.test import boot as B
from port.test import lib as L

pytestmark = pytest.mark.port

THEME, PALETTE = "cov_r_theme_cancel", "cov_r_palette_hot"
CHECKPOINTS = ("sandbox", "hover_undo", "undo_help", "hover_wrench", "amenities")
SPRITE_84, SPRITE_88 = 84, 88            # SpriteRef index = hit-test code * 4: 0x15 and 0x16 (showInfoCursorPanel)


@pytest.fixture(scope="module")
def portlib():
    return L.load()


def _need(scen):
    if not (B.TRACES / f"{scen}.trace").exists() or B.expected_records(scen)[1] is None:
        pytest.skip(f"no trace or capture for {scen} (port/corpus/traces, log/p2/p24; re/tools/segment_corpus.py capture + trace)")


@pytest.mark.parametrize("scen", (THEME, PALETTE))
def test_boot_passes(portlib, scen):
    _need(scen)
    r = B.run(scen, portlib)
    assert r.status == "PASS", f"{scen}: {r.status} {r.detail} | {r.run_detail[:300]}"
    assert r.records == r.expected and r.records > 100


def test_theme_cancel_control_reads_red(portlib):
    """Cancel keeping the theme leaves g_theme = the pressed row where the original has Standard: RED at the first record."""
    _need(THEME)
    portlib.negctl("showThemePackSelect", 2)
    try:
        r = B.run(THEME, portlib)
    finally:
        portlib.negctl_reset()
    assert r.status == "FAIL" and r.first_divergence["record"] == 0, f"{r.status} {r.detail}"
    assert "g_theme" in r.first_divergence["objects"]


def _cuts(img, base):
    return [struct.unpack_from("<3i", img, base + 0x2c * i + 0x18) for i in range(96)]


def _checkpoint_record(scen, name):
    """The boot record whose clock value is the capture's frame-head ret value for the checkpoint (`ret` of <name>.fh.json)."""
    meta = json.loads((B.CAPTURES / scen / "c_a" / f"{name}.fh.json").read_text())
    _, exp = B.expected_records(scen)
    return [t for t, _ in exp].index(meta["ret"])


@pytest.mark.parametrize("name", CHECKPOINTS)
def test_palette_cuts_equal_the_original_after_the_hotspots(portlib, name):
    """All 96 palette SpriteRefs' width / height / kind (+0x18 / +0x1c / +0x28), SpriteRefs 84 and 88 included, equal the
    original's frame-head image at the checkpoint; loadInterfaceArt 3 (the two cuts skipped) differs in exactly those two."""
    _need(PALETTE)
    fh = B.CAPTURES / PALETTE / "c_a" / f"{name}.fh.bin"
    if not fh.exists():
        pytest.skip(f"no {fh}")
    want_img = fh.read_bytes()
    k = _checkpoint_record(PALETTE, name)
    base = [x for n, x, _ in portlib.objects("pointer") if n == "g_sprites570cb8"][0]
    want = _cuts(want_img, base)
    assert want[SPRITE_84][:2] == (0x22, 0x28) and want[SPRITE_88][:2] == (0x22, 0x22), (want[SPRITE_84], want[SPRITE_88])

    def boot_cuts():
        out = {}
        B.run(PALETTE, portlib, max_records=k + 1, after=lambda st, res: out.setdefault("img", st.image()))
        return _cuts(out["img"], base)

    assert boot_cuts() == want
    portlib.negctl("loadInterfaceArt", 3)
    try:
        got = boot_cuts()
    finally:
        portlib.negctl_reset()
    assert [i for i in range(96) if got[i] != want[i]] == [SPRITE_84, SPRITE_88]
