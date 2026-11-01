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

import ctypes

import numpy as np
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


# ------------------------------------------------------------------------------------------------ roadmap S2.1: the family table
# A test-only body family (port/src/sim/gfx_selftest.cpp, listed in gfx_families.inc) is dispatched through the same table as
# every real body. Removing its line from gfx_families.inc makes these tests RED: gfx.family.selftest is gone and gfx.call
# reports `no body: gfx_selftest.fillRoot` instead of drawing (log/roadmap/S2/1_notes.md, the removal control).
NAME_VA = 0x30000000
ROOT_W, ROOT_H = 0x320, 0x258
SELFTEST_NEGCTL = [(1, "fillRoot leaves the rectangle's last column unfilled"), (2, "loadCopy copies to (x + 1, y)")]
# kNotDrawing (gfx.cpp): drops that draw nothing, never reported unsupported (S0 recommendation 1; framegate.py NOT_DRAWING)
NOT_DRAWING = ("waitTicks", "startSoundSlot", "playSound", "Snd::status", "capsLockOff", "emptyFunc_4a0890", "saveCourseJpg",
               "enterModal483d30")


@pytest.fixture(scope="module")
def flib():
    lib = L.load()
    if not lib.has("gfx.attach"):
        pytest.skip("the library has no gfx module")
    return lib


def _gstate(lib):
    st = lib.state()
    st._namebuf = st.map(NAME_VA, bytearray(0x1000))
    st.call("gfx.attach", [])
    return st


def _put_str(st, off: int, s: str) -> int:
    raw = s.encode() + b"\0"
    ctypes.memmove(ctypes.addressof(st._namebuf) + off, raw, len(raw))
    return NAME_VA + off


def _gcall(st, name: str, args=()) -> int:
    _put_str(st, 0, name)
    return st.call("gfx.call", [NAME_VA] + list(args) + [0] * (11 - len(args))).eax


def _root(st):
    assert st.call("gfx.dump", [1]).eax == 0
    raw = st.files_read("$gfx/root.555")
    return np.frombuffer(raw, "<u2").reshape(ROOT_H, ROOT_W)


def _log(st) -> str:
    st.call("gfx.log", [])
    return st.files_read("$gfx/log.txt").decode()


def _pcx(w: int, h: int, rows: list[list[int]], pal: dict[int, tuple[int, int, int]]) -> bytes:
    """A synthetic 8-bit PCX (version 5, RLE, one plane; literal bytes < 0xc0 only) with the given palette entries."""
    bpl = w + (w & 1)
    hdr = bytearray(0x80)
    hdr[0], hdr[1], hdr[2], hdr[3] = 0xA, 5, 1, 8
    hdr[8:10] = (w - 1).to_bytes(2, "little")
    hdr[10:12] = (h - 1).to_bytes(2, "little")
    hdr[65] = 1
    hdr[66:68] = bpl.to_bytes(2, "little")
    body = bytearray()
    for r in rows:
        body += bytes(r + [0] * (bpl - len(r)))
    p = bytearray(0x300)
    for k, (r, g, b) in pal.items():
        p[3 * k:3 * k + 3] = bytes((r, g, b))
    return bytes(hdr) + bytes(body) + b"\x0c" + bytes(p)


def _rgb555(r, g, b):
    return ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3)


def test_family_table_registers_selftest(flib):
    assert flib.has("gfx.family.selftest"), "gfx_families.inc does not list the selftest family"
    with _gstate(flib) as st:
        assert st.call("gfx.family.selftest", []).eax == 2
        assert st.call("gfx.check", []).eax == 0, st.files_read("$gfx/bodies.txt")
        lines = set(st.files_read("$gfx/bodies.txt").decode().splitlines())
    assert "body\tgfx_selftest.fillRoot\tselftest" in lines
    assert "body\tgfx_selftest.loadCopy\tselftest" in lines
    assert "not_drawing\tgfx_selftest.quiet\tselftest" in lines
    for core in ("showTitleScreen", "loadImage", "Image_draw", "Window::refreshFull"):
        assert f"body\t{core}\tgfx" in lines, core
    for nd in NOT_DRAWING:
        assert f"not_drawing\t{nd}\tgfx" in lines, nd


def test_selftest_fill_is_dispatched(flib):
    with _gstate(flib) as st:
        assert _gcall(st, "gfx_selftest.fillRoot", [10, 20, 5, 3, 0x1234]) == 1
        img = _root(st)
        log = _log(st)
    assert (img[20:23, 10:15] == 0x1234).all()
    assert int((img == 0x1234).sum()) == 15
    assert "handled\tgfx_selftest.fillRoot\t1" in log


def test_selftest_load_copy_uses_the_shared_helpers(flib):
    pal = {1: (0xff, 0, 0), 2: (0, 0x80, 0), 3: (8, 16, 0xf8)}
    data = _pcx(3, 2, [[1, 2, 3], [3, 2, 1]], pal)
    with _gstate(flib) as st:
        st.files_put("selftest.pcx", data)
        assert _gcall(st, "gfx_selftest.loadCopy", [_put_str(st, 0x100, "selftest.pcx"), 40, 50]) == 1
        img = _root(st)
        log = _log(st)
    assert "unsupported\t" not in log, log
    want = [[_rgb555(*pal[i]) for i in row] for row in ([1, 2, 3], [3, 2, 1])]
    assert img[50:52, 40:43].tolist() == want


@pytest.mark.parametrize("mode,what", SELFTEST_NEGCTL)
def test_selftest_negctl_red(flib, mode, what):
    prev = flib.negctl("gfx.family.selftest", mode)
    try:
        with _gstate(flib) as st:
            st.files_put("selftest.pcx", _pcx(3, 2, [[1, 2, 3], [3, 2, 1]], {1: (0xff, 0, 0), 2: (0, 0x80, 0), 3: (8, 16, 0xf8)}))
            _gcall(st, "gfx_selftest.fillRoot", [10, 20, 5, 3, 0x1234])
            _gcall(st, "gfx_selftest.loadCopy", [_put_str(st, 0x100, "selftest.pcx"), 40, 50])
            img = _root(st)
    finally:
        flib.negctl("gfx.family.selftest", prev or 0)
    fill_ok = int((img == 0x1234).sum()) == 15 and (img[20:23, 10:15] == 0x1234).all()
    copy_ok = img[50, 40] == _rgb555(0xff, 0, 0) and img[51, 42] == _rgb555(0xff, 0, 0)
    assert not (fill_ok and copy_ok), (mode, what)


def test_not_drawing_drops_are_never_unsupported(flib):
    with _gstate(flib) as st:
        for nd in NOT_DRAWING + ("gfx_selftest.quiet",):
            assert _gcall(st, nd) == 0, nd
        assert st.call("gfx.log", []).eax == 0, _log(st)


def test_log_lists_every_reason_in_first_occurrence_order(flib):
    # S0 recommendation 3: no 64 cap; `first\t<k>\t<reason>` in the order the reasons first occurred (framegate.py reads them)
    names = [f"gfx_selftest.none{k:03d}" for k in range(80, 0, -1)]   # descending, so a sorted list would differ
    with _gstate(flib) as st:
        for n in names:
            assert _gcall(st, n) == 0
        _gcall(st, names[5])                                              # a repeat adds no line
        n_unsup = st.call("gfx.log", []).eax
        log = st.files_read("$gfx/log.txt").decode()
    first = [ln.split("\t") for ln in log.splitlines() if ln.startswith("first\t")]
    assert n_unsup == len(names)
    assert [int(f[1]) for f in first] == list(range(len(names)))
    assert [f[2] for f in first] == [f"no body: {n}" for n in names]
    assert f"unsupported\tno body: {names[5]}\t2" in log


def test_record_budget_runs_one_frame_past_the_checkpoint():
    # S0 recommendation 4: FRAME records at or before the last checkpoint's ordinal + 2; past the trace's last FRAME, to_end
    if not (B.TRACES / f"{SCEN}.trace").exists() or not (GX.REFS / SCEN / "det_b" / "summary.json").exists():
        pytest.skip("no trace or pinned reference for sandbox_basic")
    import trace_format as TF
    tr = TF.read(B.TRACES / f"{SCEN}.trace")
    pos = [r.f["pos"] for r in tr.records if r.type == TF.FRAME]
    o = GX.checkpoint_ordinal(SCEN, CHK)
    mr, to_end = GX.record_budget(SCEN, o)
    assert mr == sum(1 for p in pos if p <= o) + 2
    assert to_end is (mr > len(pos))
    mr2, to_end2 = GX.record_budget(SCEN, pos[-1] + 1)
    assert mr2 == len(pos) + 2 and to_end2 is True
    assert GX.record_budget(SCEN, o, max_records=7) == (7, False)
