"""Roadmap S2.7: the boot screens: the loading screen (showLoadingScreen), Select Difficulty (selectDifficultyScreen), the theme
packs (showThemePackSelect), the championship course list (loadGameBrowserScreen, S2.4's) and Pick A Pro (pickAProScreen), on
the pinned scenario cov_r_s2_screens (S2.6).

Bodies: family `boot_screens` (port/src/sim/gfx_boot_screens.cpp), plus the screens' drawing calls in menu.cpp
(show_theme_pack_select, pick_a_pro_screen, load_flic's refreshFull and waitTicks after the loading screen),
commands_screens.inc (show_loading_screen, select_difficulty_screen) and tick_boot.inc (Setup's title art), all through
log/roadmap/S2/7_merge.md. Notes: log/roadmap/S2/7_notes.md.

FRAMES: loading_t200, loading_t300, theme_packs, difficulty, champ_courses and pick_a_pro equal the original's back buffer in
every pixel (menu and title_back stay equal).

U-1252 / U-1260 (theme_packs): showThemePackSelect reads its sixth column word for flag bit 5 without ever writing it, so the
current theme's row gets one more cell at x = w - 0x2b, w whatever the stack held. The pinned runs (no trace recorder) have w =
0x1a: the 87-pixel cell covers x 0..69 of the Standard row; the trace recorder's runs t_rec / t_rep1 show no such cell. The
backend uses w = 0x1a; SG_GFX_THEMEPACK_COL5 overrides it. With no cell the frame equals t_rec and t_rep1 in every pixel and
differs from det_b in exactly those 2,170 pixels; a debug-fill word (0xcccc as a signed word) puts the cell off the screen.

Controls RED on the frames: boot_screens 1 2 (loading), 3 4 (difficulty), 5 6 (theme packs), 8 13 (pick a pro), 10 11 12 (the
boot's title art, loadFlic's refreshFull and wait). By unit tests (no checkpoint shows them): 7 (the theme hover), 9 (the
difficulty tip).
"""
from __future__ import annotations

import ctypes
import os

import numpy as np
import pytest

from port.test import gfx as GX
from port.test import lib as L
from port.test import segments as SG

pytestmark = pytest.mark.port

SCEN = "cov_r_s2_screens"
SCREENS = ["loading_t200", "loading_t300", "theme_packs", "difficulty", "champ_courses", "pick_a_pro"]
CHECKS = SCREENS + ["menu", "title_back"]
FAMILY = "gfx.family.boot_screens"
WIN32 = os.environ.get("SG_BACKEND", "x64").lower() == "win32"

NAME_VA = 0x30000000
W, H = 0x320, 0x258
STRIP = (0, 156, 69, 186)    # the column-5 cell of the Standard row in det_b (x0, y0, x1, y1)


@pytest.fixture(scope="module")
def blib():
    lib = L.load()
    if not lib.has(FAMILY):
        pytest.skip("the library has no boot_screens family")
    if not lib.exe_loaded() and lib.exe_load():
        pytest.skip("no exe image (original/golf_clean.exe or SG_EXE_PATH)")
    return lib


def _have():
    return (GX.REFS / SCEN / "det_b" / "theme_packs.png").exists() and SG.INSTALL.is_dir()


@pytest.fixture(scope="module")
def screens_run(blib):
    if not _have() or not blib.has("gditext.attach"):
        pytest.skip("no pinned reference / install / host text provider")
    return GX.render(SCEN, CHECKS, blib)


def _png(path):
    from PIL import Image
    return np.asarray(Image.open(path).convert("RGB"))


def _diff(img, ref):
    d = (img != ref).any(2)
    if not d.any():
        return 0, None
    ys, xs = np.nonzero(d)
    return int(d.sum()), (int(xs.min()), int(ys.min()), int(xs.max()), int(ys.max()))


# ------------------------------------------------------------------------------------------------ the frames
@pytest.mark.parametrize("chk", CHECKS)
def test_frame_equals_the_original(screens_run, chk):
    r = GX.compare(screens_run, chk)
    assert r["total"] == 0, (chk, r)


def test_boot_records_equal_with_the_backend(screens_run):
    # the drawing writes nothing the boot compares: every record up to the run's budget is equal
    assert screens_run.boot.status in ("PASS", "KNOWN"), screens_run.boot.detail


def test_bodies_handled(screens_run):
    log = screens_run.log
    for name in ("showLoadingScreen.draw", "selectDifficultyScreen.setup", "selectDifficultyScreen.redraw",
                 "selectDifficultyScreen.teardown", "showThemePackSelect.setup", "showThemePackSelect.redraw",
                 "showThemePackSelect.teardown", "pickAProScreen.setup", "pickAProScreen.teardown"):
        assert f"handled\t{name}\t" in log, name
    for prefix in ("showLoadingScreen", "selectDifficultyScreen", "showThemePackSelect", "pickAProScreen",
                   "gfx_boot_screens", "drawTextSurface"):
        assert not any(l.startswith("unsupported\t" + prefix) for l in log.splitlines()), prefix


def test_family_registered_without_duplicates(blib):
    with blib.state() as st:
        st.set_data_base(SG.DATA_VA)
        assert st.call(FAMILY, []).eax == 13
        assert st.call("gfx.check", []).eax == 0, st.files_read("$gfx/bodies.txt")


# ------------------------------------------------------------------------------------------------ U-1252 / U-1260
def _render_col5(lib, value):
    old = os.environ.get("SG_GFX_THEMEPACK_COL5")
    os.environ["SG_GFX_THEMEPACK_COL5"] = value
    try:
        return GX.render(SCEN, ["theme_packs"], lib)
    finally:
        if old is None:
            del os.environ["SG_GFX_THEMEPACK_COL5"]
        else:
            os.environ["SG_GFX_THEMEPACK_COL5"] = old


@pytest.fixture(scope="module")
def trec():
    p = SG.ROOT / "log" / "p2" / "p24" / SCEN / "t_rec" / "theme_packs.png"
    if not p.exists():
        pytest.skip("no trace-recorder frame")
    return _png(p)


@pytest.mark.skipif(WIN32, reason="the out-of-process runner inherits its environment at spawn")
@pytest.mark.parametrize("value", ["none", str(-0x3334)])
def test_theme_packs_without_column_5_cell_equals_the_trace_run(screens_run, blib, trec, value):
    run = _render_col5(blib, value)
    img = run.frames["theme_packs"]
    assert _diff(img, trec)[0] == 0
    rep = SG.ROOT / "log" / "c3" / "alt" / "refs" / SCEN / "t_rep1" / "theme_packs.png"
    if rep.exists():
        assert _diff(img, _png(rep))[0] == 0
    n, box = _diff(img, GX.reference(SCEN, "theme_packs"))
    assert n == 2170 and box == STRIP, (n, box)


@pytest.mark.skipif(WIN32, reason="the out-of-process runner inherits its environment at spawn")
def test_theme_packs_column_5_word_moves_the_cell(screens_run, blib):
    run = _render_col5(blib, "0x1b")    # one pixel right of the pinned runs' word
    n, box = _diff(run.frames["theme_packs"], GX.reference(SCEN, "theme_packs"))
    assert n > 0 and box[1] >= STRIP[1] and box[3] <= STRIP[3], (n, box)


def test_trace_run_differs_from_the_pinned_runs_only_in_the_strip(trec):
    n, box = _diff(trec, GX.reference(SCEN, "theme_packs"))
    assert n == 2170 and box == STRIP, (n, box)


# ------------------------------------------------------------------------------------------------ controls on the frames
FRAME_NEGCTL = [
    (1, "loading_t200", "the bar's yellow part one pixel wider"),
    (2, "loading_t300", "the hint without its shadow"),
    (3, "difficulty", "captions not lowered"),
    (4, "difficulty", "the screen from the highlight sheet"),
    (5, "theme_packs", "column titles in the row font"),
    (6, "theme_packs", "the characters probe skipped"),
    (8, "pick_a_pro", "element 10 not cut"),
    (10, "loading_t200", "no title art under the loading screen"),
    (11, "loading_t300", "no refreshFull after loadFlic's loading screen"),
    (12, "loading_t200", "loadFlic's wait reads not consumed"),
    (13, "pick_a_pro", "rows one pixel right"),
]


@pytest.mark.parametrize("mode,chk,what", FRAME_NEGCTL)
def test_frames_negctl_red(blib, screens_run, mode, chk, what):
    run = GX.render(SCEN, [chk], blib, negctl=mode, negctl_name=FAMILY)
    assert GX.compare(run, chk)["total"] > 0, (mode, chk, what)


# ------------------------------------------------------------------------------------------------ unit tests
def _offset(lib, name):
    for n, off, _size in lib.objects():
        if n == name:
            return off
    raise KeyError(name)


def _negctl(lib, mode):
    class _Ctl:
        def __enter__(self):
            self.prev = lib.negctl(FAMILY, mode)

        def __exit__(self, *e):
            lib.negctl(FAMILY, self.prev or 0)
    return _Ctl()


def _state(lib):
    st = lib.state()
    st.set_data_base(SG.DATA_VA)
    st.fill_from_exe(True)
    st.files_root(SG.INSTALL)
    st._namebuf = st.map(NAME_VA, bytearray(0x1000))
    # the root surface as the boot leaves it: the root window's Surface part (+0x274), what fillWindowRect draws into
    st.write(_offset(lib, "f_stateptr_4c1570"), _root_va(lib).to_bytes(4, "little"))
    st.call("gfx.attach", [])
    return st


def _root_va(lib):
    return SG.DATA_VA + _offset(lib, "g_rootWindow") + 0x274


def _g(st, name, args=()):
    raw = name.encode() + b"\0"
    ctypes.memmove(ctypes.addressof(st._namebuf), raw, len(raw))
    return st.call("gfx.call", [NAME_VA] + [a & 0xFFFFFFFF for a in args] + [0] * (11 - len(args))).eax


def _back(st):
    assert st.call("gfx.dump", [0]).eax == 0
    return np.frombuffer(st.files_read("$gfx/back.555"), "<u2").reshape(H, W).copy()


def _log(st):
    st.call("gfx.log", [])
    return st.files_read("$gfx/log.txt").decode()


@pytest.fixture(scope="module")
def ulib(blib):
    if not SG.INSTALL.is_dir() or not blib.has("gditext.attach"):
        pytest.skip("no install or no host text provider")
    return blib


def _hover(lib, mode=0):
    with _negctl(lib, mode), _state(lib) as st:
        _g(st, "showThemePackSelect.setup", [0])
        _g(st, "showThemePackSelect.hover", [-1, 0])
        img = _back(st)
        _g(st, "showThemePackSelect.teardown")
        return img, _log(st)


def test_theme_hover_draws_the_highlighted_button(ulib):
    img, log = _hover(ulib)
    assert "unsupported\t" not in log, log
    assert img[0x208:0x208 + 0x50, 0x245:0x245 + 0x47].any()            # the load button's highlighted image
    assert not img[:0x200].any()                                       # nothing else


def test_theme_hover_negctl_7_red(ulib):
    good, _ = _hover(ulib)
    bad, _ = _hover(ulib, 7)
    assert bad.tolist() != good.tolist()


def _tip(lib, mode=0):
    with _negctl(lib, mode), _state(lib) as st:
        st.call("gditext.attach", [])
        try:
            st.write(_offset(lib, "g_fontRef_519fd8") + 4, (1).to_bytes(4, "little"))   # a font handle: drawText's +4 guard
            _g(st, "selectDifficultyScreen.tip")
            _g(st, "Window::refreshFull", [SG.DATA_VA + _offset(lib, "g_rootWindow"), 0])
            return _back(st), _log(st)
        finally:
            st.call("gditext.detach", [])


def test_difficulty_tip_draws_cancel_near_the_back_button(ulib):
    img, log = _tip(ulib)
    ys, xs = np.nonzero(img == 0x7FFF)
    assert len(xs) > 10, log
    assert 0x2ff - 40 < xs.mean() < 0x2ff + 40 and 0x22d - 5 <= ys.min() and ys.max() < 0x22d + 30


def test_difficulty_tip_negctl_9_red(ulib):
    good, _ = _tip(ulib)
    bad, _ = _tip(ulib, 9)
    assert bad.tolist() != good.tolist()


def _loading(lib, percent, r, r2):
    with _state(lib) as st:
        st.call("gditext.attach", [])
        try:
            st.write(_offset(lib, "g_font51a028") + 4, (1).to_bytes(4, "little"))
            _g(st, "setDrawTarget", [_root_va(lib), SG.DATA_VA + _offset(lib, "g_font51a028"), 0, 0, 0])
            _g(st, "showLoadingScreen.draw", [percent, r, r2])
            _g(st, "Window::refreshFull", [SG.DATA_VA + _offset(lib, "g_rootWindow"), 0])
            return _back(st), _log(st)
        finally:
            st.call("gditext.detach", [])


def test_loading_without_percent_draws_no_bar(ulib):
    with_bar, _ = _loading(ulib, 50, 3, 4)
    no_bar, _ = _loading(ulib, -1, 3, 4)
    band = (slice(0x190, 0x190 + 0x18), slice(0x12b, 0x12b + 0xca))
    assert (with_bar[band] == 0x7FFF).any() and not (no_bar[band] == 0x7FFF).all()
    assert with_bar[:0x190].tolist() == no_bar[:0x190].tolist()          # the picture and the hint are the same


def test_loading_picture_follows_the_first_draw(ulib):
    a, _ = _loading(ulib, -1, 3, 11)     # hint 11: none (the original's table has 0..10)
    b, _ = _loading(ulib, -1, 4, 11)
    pic = (slice(0xaf, 0xaf + 0xca), slice(0x5a, 0x5a + 0x26c))
    assert a[pic].any() and a[pic].tolist() != b[pic].tolist()
    c, _ = _loading(ulib, -1, 12, 11)    # r 12: no picture loaded (only 0..11 have one)
    assert not c[pic].any()
