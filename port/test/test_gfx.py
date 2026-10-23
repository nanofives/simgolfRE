"""Phase 5, step 1 (re/PHASE5_GFX_PLAN.md, port/test/gfx.py): the software 16-bit graphics backend against the original's frames.

The port runs sandbox_basic from process start (port/test/boot.py) with the backend attached (gfx.attach) and snapshots its
back buffer at the clock call where the pinned reference took its `menu` checkpoint; the snapshot must equal
log/c3/alt/refs/sandbox_basic/det_b/menu.png in every pixel and in every region (each title button, each label, the
background around them), with no unsupported drawing call before it. The images come from the user's install at run time
(interface\\TitleBASE.pcx, TitleUnSel.pcx, TitleMO.pcx and two window-frame files) and the labels from the host GDI text
provider (gditext), as the original draws them with GDI.

Negative controls (port/src/sim/gfx.cpp, g_negctl_gfx), each RED against the reference: 1 Image_draw one pixel to the
right, 2 one palette entry's RGB555 word off by one, 4 Window::refreshFull not copying the root surface to the back buffer.
NEGCTL_BLIND: 3 (draw16's transparency test `> 0xfe`) cannot change this frame: none of the three title images holds index
0xfe (measured: 0 bytes of 0xfe in TitleBASE / TitleUnSel / TitleMO), so the frames that judge it come later.

The backend writes nothing into the GameState: with it attached the boot run's per-frame sim hashes still equal the
original's capture for the whole of sandbox_basic.

THE LOCATION SCREEN (courseActionMenu, cov_b_location): its five checkpoints refused (the insufficient-funds dialog:
drawWindow -> drawFrameR3's shaded panel and blended border, drawMessageBuffer), site_hover (the nearest site's wide plate and
its three lines, the blinking pin), buttons (Save / Load after the scenario's date write), tip (drawPanelBackdrop's label)
and back (the title menu again) equal the original in every pixel. The capture has no frame record, so the boot runs over the
whole trace (boot.run to_end) and the last snapshot is taken where the recorded input runs out. Controls: gfx 5 (the pin and
border blend ignores its mask) RED on buttons, gfx 6 (the panel's shade byte drawn as a colour) RED on refused, tick.frame 7
(the harness's writes no longer applied at a modal loop's cursor reads) RED on buttons (the date stays 0, one button).
"""
from __future__ import annotations

import pytest

from port.test import boot as B
from port.test import gfx as GX
from port.test import lib as L
from port.test import segments as SG

pytestmark = pytest.mark.port

SCEN, CHK = "sandbox_basic", "menu"
NEGCTL = [(1, "Image_draw one pixel to the right (a wrong blit)"),
          (2, "a loaded image's palette word for its top-left pixel's index off by one (a wrong palette entry)"),
          (4, "Window::refreshFull does not present the root surface")]
NEGCTL_BLIND = [(3, "draw16 draws index 0xfe: the title images hold no byte 0xfe")]


@pytest.fixture(scope="module")
def portlib():
    lib = L.load()
    if not lib.has("gfx.attach"):
        pytest.skip("the library has no gfx module")
    if lib.exe_load():
        pytest.skip("no exe image (original/golf_clean.exe or SG_EXE_PATH)")
    if not SG.INSTALL.is_dir():
        pytest.skip("no install (original/)")
    if not (B.TRACES / f"{SCEN}.trace").exists() or not (GX.REFS / SCEN / "det_b" / f"{CHK}.png").exists():
        pytest.skip("no trace or pinned reference for sandbox_basic")
    return lib


@pytest.fixture(scope="module")
def menu_run(portlib):
    if not portlib.has("gditext.attach") or "drawCenteredText: no text provider" in (run := GX.render(SCEN, [CHK], portlib)).log:
        pytest.skip("no host text provider (gditext: Windows GDI)")
    return run


def test_checkpoint_ordinal_is_the_checkpoint_clock_call():
    # summary.json: menu T 1052010 at tgt 3434; the trace's tgt call 3434 carries that value at clock ordinal 16585
    assert GX.checkpoint_ordinal(SCEN, CHK) == 16585


def test_menu_frame_equals_the_original(menu_run):
    assert menu_run.frames.get(CHK) is not None, menu_run.log
    r = GX.compare(menu_run, CHK, GX.menu_regions())
    assert r["total"] == 0, r
    assert all(v == 0 for v in r["regions"].values()), r


def test_nothing_unsupported_before_the_snapshot(menu_run):
    assert menu_run.unsupported_before.get(CHK) == 0, menu_run.log
    for name in ("showTitleScreen", "loadImage", "Surface_blit3", "Image_draw", "drawCenteredText", "Window::refreshFull"):
        assert f"handled\t{name}\t" in menu_run.log, (name, menu_run.log)


def test_boot_records_unchanged_with_the_backend(menu_run):
    assert menu_run.boot.status == "PASS", menu_run.boot.detail


@pytest.mark.parametrize("mode,what", NEGCTL)
def test_negctl_red(portlib, mode, what):
    run = GX.render(SCEN, [CHK], portlib, negctl=mode)
    r = GX.compare(run, CHK, GX.menu_regions())
    assert r["total"] > 0, (mode, what)


@pytest.mark.parametrize("mode,what", NEGCTL_BLIND)
def test_negctl_blind_is_equivalent_here(portlib, mode, what):
    # documented blind spot: the control must stay GREEN on this frame (if it turns RED, move it to NEGCTL)
    run = GX.render(SCEN, [CHK], portlib, negctl=mode)
    assert GX.compare(run, CHK)["total"] == 0, (mode, what)


LOC = "cov_b_location"
LOC_CHK = ("refused", "site_hover", "buttons", "tip", "back")
LOC_NEGCTL = [("gfx.attach", 5, "the pin and border blend ignores its mask", "buttons"),
              ("gfx.attach", 6, "the panel's shade byte drawn as a palette colour", "refused"),
              ("tick.frame", 7, "the harness's memory writes not applied at a modal loop's cursor reads", "buttons")]


@pytest.fixture(scope="module")
def loc_run(portlib):
    if not (B.TRACES / f"{LOC}.trace").exists() or not (GX.REFS / LOC / "det_b" / "buttons.png").exists():
        pytest.skip("no trace or pinned reference for cov_b_location")
    return GX.render(LOC, list(LOC_CHK), portlib)


@pytest.mark.parametrize("chk", LOC_CHK)
def test_location_frames_equal_the_original(loc_run, chk):
    assert loc_run.frames.get(chk) is not None, loc_run.log
    r = GX.compare(loc_run, chk)
    assert r["total"] == 0, (chk, r)
    assert loc_run.unsupported_before.get(chk) == 0, loc_run.log


@pytest.mark.parametrize("name,mode,what,chk", LOC_NEGCTL)
def test_location_negctl_red(portlib, loc_run, name, mode, what, chk):
    run = GX.render(LOC, [chk], portlib, negctl=mode, negctl_name=name)
    assert GX.compare(run, chk)["total"] > 0, (name, mode, what)


def test_whole_boot_with_the_backend_attached(portlib):
    run = GX.render(SCEN, [CHK], portlib, max_records=10**6)
    assert run.boot.status == "PASS", run.boot.detail
    assert run.boot.records == run.boot.expected
