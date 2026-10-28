"""Roadmap S2.9: the dialogs drawn over the course: showPopup (every popup and the System menu), textInputBox (with
saveGameDialog's panel) and the association's grading page of showCourseReport.

Bodies (family `dialogs`, port/src/sim/gfx_dialogs.cpp; notes log/roadmap/S2/9_notes.md): loadInterfaceArt.dialogArt (the
popup border, its masks, the option bullets, the icon row with the report's star), drawPanelS2, drawTextSurface,
showPopup's odd-y box and the dialog markers; the screens' calls are in commands_popup.inc, saveload.cpp and reports.cpp.
Widget_fillRect (the caret), Surface_drawSprite (drawAlpha's 1/4 band), Widget_draw, drawStringAt and drawStringAt2 are the
`reports` family's bodies (gfx_reports.cpp; roadmap S2.13 kept one body per name, U-1278): their controls here are that family's
modes 9 (the line one pixel left), 1 (a half blend), 10 (drawStringAt at x - w), 20 (Widget_draw one right) and 21
(drawStringAt2 left-aligned), the dialogs modes 3, 7, 8, 10, 11 they replace.

The course under a dialog is not drawn yet (roadmap S3), so each dialog is judged in its own rectangle, measured on the
reference frame (log/c3/alt/refs/<scenario>/det_b/<checkpoint>.png): a popup or panel rectangle is the bounding box of the
connected region of its fill colour 0x4e79 (RGB (156, 156, 206) after the back buffer's (v << 3) | (v >> 2) expansion; a
region of 2,000 pixels or more) grown by 5 pixels on every side (the border pieces start 4 pixels outside the fill, which they
overlap by one); the grading page's is the bounding box of its panel colour RGB (148, 148, 198) grown by 5. A partly covered
dialog's rectangle is the part of its fill still visible.

HELD pixels: a pixel of a rectangle that depends on what lies under the dialogs. The family's probe (negctl modes >= 0x10000,
a measurement, not a control) fills the root surface when a dialog stack starts; three runs (the colours 0x0000 and 0x7fff and
a pseudo-random pattern) and the run without a probe agree on a pixel when the dialogs alone decide it (with the two colours
alone, k_dlg_J's twice-blended corner pixel (682, 431) agreed by coincidence) (the blended
border pieces, the transparent corners, the grading page's drawAlpha pass and transparent parts, the odd-y popup colour do
not). The gate: every non-held pixel of every rectangle equals the reference; held pixels are reported (until S3 draws the
course).
"""
from __future__ import annotations

import ctypes
import sys

import numpy as np
import pytest

from port.test import gfx as GX
from port.test import lib as L
from port.test import segments as SG

pytestmark = pytest.mark.port

# (x0, y0, x1, y1) inclusive, measured on the reference frames as the module docstring says
CASES = {
    "cov_r_quit": {
        "exit_prompt": [(240, 100, 559, 243), (82, 337, 417, 559)],              # retirement popup, System menu
        "exit_save": [(49, 80, 687, 159), (240, 155, 559, 243), (82, 337, 417, 559)],   # + SAVE GAME panel and text box
    },
    "cov_a_keys": {
        "k_dlg_S": [(49, 80, 687, 159)],                                           # SAVE GAME panel and text box
        "k_dlg_J": [(34, 25, 766, 444)],                                           # the association's grading page
        "k_C": [(192, 137, 607, 263)],                                             # overwrite popup
    },
    "cov_a_keys2": {"C_prompt": [(192, 137, 607, 263)]},
    "cov_a_champ": {"n_prompt": [(256, 137, 543, 263)]},
    "cov_a_play": {"n_prompt": [(272, 137, 527, 263)]},
    "cov_a_build2": {"remove_prompt1": [(248, 97, 551, 223)]},
    "cov_k3_career": {"prompt": [(66, 147, 433, 273)], "prompt2": [(66, 147, 433, 273)]},
}
# Pixels held by measurement where the probes cannot see the dependence: k_dlg_J (682, 431) has SGAreport_alpha byte 52 there (a
# blend with the destination in blitPair1: ((dst8 * 52) >> 8 + src8) >> 3 per channel), but the page is drawn twice in this
# stack (showCourseReport(2) runs twice) and the port's two drawAlpha + blend passes give the same word for every background
# tried (U-1280)
HELD_EXTRA = {("cov_a_keys", "k_dlg_J"): [(682, 431)]}
PROBES = (0x10000, 0x17FFF, 0x20001)   # black, white, a pseudo-random pattern
FAMILY = "gfx.family.dialogs"
REPORTS = "gfx.family.reports"   # owner of the shared primitives (U-1278)


@pytest.fixture(scope="module")
def glib():
    lib = L.load()
    if not lib.has(FAMILY):
        pytest.skip("the library has no dialogs family")
    if not lib.exe_loaded() and lib.exe_load():
        pytest.skip("no exe image (original/golf_clean.exe or SG_EXE_PATH)")
    return lib


def _have(scen):
    return all((GX.REFS / scen / "det_b" / f"{c}.png").exists() for c in CASES[scen]) and SG.INSTALL.is_dir()


def render(lib, scen, mode=0, name=FAMILY):
    return GX.render(scen, list(CASES[scen]), lib, negctl=mode, negctl_name=name)


def measure(lib, scen, normal=None) -> dict:
    """{checkpoint: [ {rect, px, opaque, fail, held, held_differ} ]} for the run without a probe and the two probe runs."""
    normal = normal or render(lib, scen)
    probes = [render(lib, scen, m) for m in PROBES]
    out = {}
    for c, rects in CASES[scen].items():
        img = normal.frames.get(c)
        if img is None or any(p.frames.get(c) is None for p in probes):
            out[c] = None
            continue
        ref = GX.reference(scen, c)
        rows = []
        for (x0, y0, x1, y1) in rects:
            sl = np.s_[y0:y1 + 1, x0:x1 + 1]
            opaque = np.ones(img[sl].shape[:2], bool)
            for p in probes:
                opaque &= (p.frames[c][sl] == img[sl]).all(2)
            for (hx, hy) in HELD_EXTRA.get((scen, c), []):
                if x0 <= hx <= x1 and y0 <= hy <= y1:
                    opaque[hy - y0, hx - x0] = False
            differ = (img[sl] != ref[sl]).any(2)
            rows.append({"rect": (x0, y0, x1, y1), "px": int(opaque.size), "opaque": int(opaque.sum()),
                         "fail": int((opaque & differ).sum()), "held": int((~opaque).sum()),
                         "held_differ": int((~opaque & differ).sum())})
        out[c] = rows
    return out


def rect_diff(run, scen, c) -> int:
    """pixels of all the checkpoint's rectangles that differ from the reference"""
    img, ref = run.frames.get(c), GX.reference(scen, c)
    if img is None:
        return -1
    return int(sum((img[y0:y1 + 1, x0:x1 + 1] != ref[y0:y1 + 1, x0:x1 + 1]).any(2).sum() for x0, y0, x1, y1 in CASES[scen][c]))


_RUNS: dict = {}


def _normal(lib, scen):
    if scen not in _RUNS:
        _RUNS[scen] = render(lib, scen)
    return _RUNS[scen]


_MEAS: dict = {}


@pytest.fixture(scope="module")
def measured(glib):
    def get(scen):
        if not _have(scen) or not glib.has("gditext.attach"):
            pytest.skip("no pinned reference / install / host text provider")
        if scen not in _MEAS:
            _MEAS[scen] = measure(glib, scen, _normal(glib, scen))
        return _MEAS[scen]
    return get


# ------------------------------------------------------------------------------------------------ the frame gate
ALL = [(s, c) for s in CASES for c in CASES[s]]


@pytest.mark.parametrize("scen,chk", ALL)
def test_dialog_rectangles_equal_the_original_outside_held_pixels(measured, scen, chk):
    rows = measured(scen)[chk]
    assert rows is not None, "no snapshot"
    for r in rows:
        assert r["fail"] == 0, r
        # the dialogs decide most of their rectangle (held: the border's blend, corners, the page's translucent parts)
        assert r["opaque"] >= 0.75 * r["px"], r


def test_dialog_bodies_handled(glib):
    run = _normal(glib, "cov_a_keys") if _have("cov_a_keys") else pytest.skip("no reference")
    for name in ("loadInterfaceArt.dialogArt", "drawPanelS2", "Widget_fillRect", "Win_fillRect", "Surface_drawSprite",
                 "Widget_draw", "drawStringAt", "Surface_stretchBlit", "dialogs.begin", "dialogs.end"):
        assert f"handled\t{name}\t" in run.log, name
    # (Widget_fillRect and Surface_drawSprite are also called by the in-game frame, whose sloped minimap lines and scene sprites
    # the backend does not model: their reasons are the frame's, not the dialogs')
    for bad in ("drawPanelS2", "drawTextSurface", "Widget_draw", "drawStringAt", "loadInterfaceArt.dialogArt",
                "no body: drawPanelS2", "no body: drawTextSurface", "Widget_fillRect: a target", "Widget_fillRect: a palette",
                "Surface_drawSprite: an alpha", "Surface_drawSprite: a destination"):
        assert not any(l.startswith("unsupported\t" + bad) for l in run.log.splitlines()), bad


def test_text_surface_used_by_a_hovered_option(glib):
    run = _normal(glib, "cov_r_quit") if _have("cov_r_quit") else pytest.skip("no reference")
    assert "handled\tdrawTextSurface\t" in run.log


# ------------------------------------------------------------------------------------------------ controls on the frames
# (family mode, scenario, checkpoint, what): RED = the checkpoint's rectangles differ from the reference in more pixels than
# the run without the control
FRAME_NEGCTL = [
    (FAMILY, 1, "cov_a_keys", "k_C", "drawPanelS2's fill colour 0x4e7a"),
    (FAMILY, 2, "cov_r_quit", "exit_prompt", "drawTextSurface without its shadow"),
    (REPORTS, 9, "cov_a_keys", "k_dlg_S", "the caret one pixel left"),
    (FAMILY, 4, "cov_a_keys", "k_C", "no right-edge border pieces"),
    (FAMILY, 5, "cov_a_keys", "k_C", "the option bullets cut one row lower"),
    (FAMILY, 6, "cov_a_keys", "k_C", "drawPanelS2 without the 16-rounding"),
    (REPORTS, 10, "cov_a_keys", "k_dlg_J", "drawStringAt centred at x - w"),
    (REPORTS, 20, "cov_a_keys", "k_dlg_J", "Widget_draw one pixel right"),
]


@pytest.mark.parametrize("fam,mode,scen,chk,what", FRAME_NEGCTL)
def test_frames_negctl_red(glib, fam, mode, scen, chk, what):
    if not _have(scen):
        pytest.skip("no reference")
    base = rect_diff(_normal(glib, scen), scen, chk)
    run = GX.render(scen, [chk], glib, negctl=mode, negctl_name=fam)
    assert rect_diff(run, scen, chk) > base, (fam, mode, what)


# ------------------------------------------------------------------------------------------------ unit tests
NAME_VA = 0x30000000
ROOT = 0x30000800
W, H = 0x320, 0x258
LOCAL = 0xFFF00000


def _offset(lib, name):
    for n, off, _size in lib.objects():
        if n == name:
            return off
    raise KeyError(name)


def _state(lib):
    st = lib.state()
    st.set_data_base(SG.DATA_VA)
    st.fill_from_exe(True)
    st._namebuf = st.map(NAME_VA, bytearray(0x1000))
    st.write(_offset(lib, "f_stateptr_4c1570"), ROOT.to_bytes(4, "little"))
    st.call("gfx.attach", [])
    return st


def _g(st, name, args=()):
    raw = name.encode() + b"\0"
    ctypes.memmove(ctypes.addressof(st._namebuf), raw, len(raw))
    return st.call("gfx.call", [NAME_VA] + [a & 0xFFFFFFFF for a in args] + [0] * (11 - len(args))).eax


def _root(st):
    assert st.call("gfx.dump", [1]).eax == 0
    return np.frombuffer(st.files_read("$gfx/root.555"), "<u2").reshape(H, W).copy()


def _negctl(lib, mode, fam=FAMILY):
    class _Ctl:
        def __enter__(self):
            self.prev = lib.negctl(fam, mode)

        def __exit__(self, *e):
            lib.negctl(fam, self.prev or 0)
    return _Ctl()


def _caret(lib, mode=0):
    with _negctl(lib, mode, REPORTS), _state(lib) as st:
        _g(st, "Widget_fillRect", [ROOT, 100, 50, 100, 60, 0x80006000])
        _g(st, "Widget_fillRect", [ROOT, 10, 5, 20, 5, 0x80000123])
        _g(st, "Widget_fillRect", [ROOT, 30, 7, 30, 7, 0x80007fff])   # a point: vline16 returns when y1 == y2
        return _root(st)


def test_widget_fill_rect_vertical_line_is_inclusive(glib):
    r = _caret(glib)
    assert (r[50:61, 100] == 0x6000).all() and r[49, 100] == 0 and r[61, 100] == 0
    assert (r[5, 10:21] == 0x0123).all() and r[5, 21] == 0 and r[5, 9] == 0
    assert r[7, 30] == 0
    assert int((r != 0).sum()) == 11 + 11


def test_widget_fill_rect_negctl_9_red(glib):   # reports mode 9 (was dialogs mode 3)
    assert not np.array_equal(_caret(glib), _caret(glib, 9))


def test_widget_fill_rect_sloped_line_is_drawn(glib):
    # the reports family's body walks line16's octants (S2.9's body reported sloped lines as unsupported)
    with _state(glib) as st:
        _g(st, "Widget_fillRect", [ROOT, 0, 0, 5, 7, 0x80000001])
        st.call("gfx.log", [])
        assert "unsupported	Widget_fillRect" not in st.files_read("$gfx/log.txt").decode()
        r = _root(st)
        assert r[0, 0] == 1 and r[7, 5] == 1 and int((r == 1).sum()) == 8   # one pixel per row of the major axis


def _blend_model(p, d):
    # draw16_1002ed60 (re/match/jgld_blit16.cpp), the member drawAlpha's quarter band calls with its flag byte set (jgld
    # 0x10018943 / 0x100189af / 0x1001907d; the reference frames show it, U-1271): RGB555 field groups, the sum shifted back
    p4, d4 = p << 4, d << 4
    ea, eb, ec, ed = p4 & 0x7C1F0, p4 & 0x3E00, d4 & 0x7C1F0, d4 & 0x3E00
    ea = (((ea + ec) >> 1) + ec) >> 1
    eb = (((eb + ed) >> 1) + ed) >> 1
    return (((ea & 0x7C1F0) + (eb & 0x3E00)) >> 4) & 0xFFFF


def _pcx(w, h, rows, pal):
    """a synthetic 8-bit PCX (version 5, RLE, one plane) with the given palette entries"""
    bpl = w + (w & 1)
    hdr = bytearray(0x80)
    hdr[0], hdr[1], hdr[2], hdr[3] = 0xA, 5, 1, 8
    hdr[8:10] = (w - 1).to_bytes(2, "little")
    hdr[10:12] = (h - 1).to_bytes(2, "little")
    hdr[65] = 1
    hdr[66:68] = bpl.to_bytes(2, "little")
    body = bytearray()
    for r in rows:
        for v in list(r) + [0] * (bpl - len(r)):
            body += bytes((0xC1, v)) if v >= 0xC0 else bytes((v,))
    p = bytearray(0x300)
    for k, (r, g, b) in pal.items():
        p[3 * k:3 * k + 3] = bytes((r, g, b))
    return bytes(hdr) + bytes(body) + b"\x0c" + bytes(p)


def _rgb555(r, g, b):
    return ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3)


ALPHA_PAL = {1: (0xF8, 0x10, 0x88), 2: (0x08, 0xF0, 0x40), 0xF8: (0xFF, 0xFF, 0xFF), 0xFF: (0, 0, 0)}
ALPHA_ROW = [1, 2, 0xF8, 0xFF]


def _alpha(lib, mode=0, alpha=0x3E800000, flag=1):
    with _negctl(lib, mode, REPORTS), _state(lib) as st:
        st.files_put("alpha.pcx", _pcx(4, 1, [ALPHA_ROW], ALPHA_PAL))
        raw = b"alpha.pcx\0"
        ctypes.memmove(ctypes.addressof(st._namebuf) + 0x100, raw, len(raw))
        _g(st, "loadImage", [LOCAL + 0x100, NAME_VA + 0x100, 0, 0, 0x100, 2])
        _g(st, "Surface_blit", [LOCAL + 0x200, LOCAL + 0x100, 0, 0, 4, 1, 1, 0])
        _g(st, "Win_fillRect", [ROOT, 0, 0, 8, 2, 0x80001234])
        _g(st, "Surface_drawSprite", [LOCAL + 0x200, ROOT, 2, 1, alpha, 0, flag])
        st.call("gfx.log", [])
        return _root(st), st.files_read("$gfx/log.txt").decode()


def test_draw_sprite_quarter_band_blends_as_the_blitter(glib):
    # draw16_1002ed60's loop (re/match/jgld_blit16.cpp) on known words: indices 1 and 2 blend, 0xf8 is skipped with the flag
    # set, 0xff always
    r, log = _alpha(glib)
    assert "unsupported\tSurface_drawSprite" not in log, log
    want = [_blend_model(_rgb555(*ALPHA_PAL[1]), 0x1234), _blend_model(_rgb555(*ALPHA_PAL[2]), 0x1234), 0x1234, 0x1234]
    assert r[1, 2:6].tolist() == want
    assert (r[0, 0:8] == 0x1234).all() and r[1, 0] == 0x1234 and r[1, 6] == 0x1234
    r0, _ = _alpha(glib, flag=0)   # without the flag 0xf8 blends too
    assert r0[1, 4] == _blend_model(_rgb555(*ALPHA_PAL[0xF8]), 0x1234)


def test_draw_sprite_other_alpha_is_unsupported(glib):
    _r, log = _alpha(glib, alpha=0x3F000000)   # 0.5: another blitter
    # S3.5: another alpha is drawn by gfxs::surface_draw_sprite (drawAlpha's window bodies), no longer reported
    assert "Surface_drawSprite: an alpha outside drawAlpha's quarter band" not in log


def test_draw_sprite_negctl_1_red(glib):   # reports mode 1, a half blend (was dialogs mode 7)
    assert not np.array_equal(_alpha(glib)[0], _alpha(glib, 1)[0])


def test_markers_probe_fills_only_at_a_new_stack(glib):
    with _negctl(glib, 0x10005), _state(glib) as st:
        _g(st, "dialogs.begin")                        # a new stack: the root becomes 5
        assert (_root(st) == 5).all()
        _g(st, "Widget_fillRect", [ROOT, 1, 1, 1, 3, 0x80000009])
        _g(st, "dialogs.end")
        _g(st, "dialogs.begin")                        # nothing drawn since: the same stack, no fill
        r = _root(st)
        assert (r[1:4, 1] == 9).all()
        _g(st, "dialogs.end")
        _g(st, "Widget_fillRect", [ROOT, 2, 1, 2, 3, 0x80000009])   # a drawing call between dialogs
        _g(st, "dialogs.begin")
        assert (_root(st) == 5).all()


def _odd(lib, mode=0):
    with _negctl(lib, mode), _state(lib) as st:
        _g(st, "Widget_fillRect", [ROOT, 40, 21, 40, 22, 0x80001ce7])     # the colour under the box's anchor (40, 21)
        _g(st, "showPopup.readColour", [40, 21])
        _g(st, "showPopup.oddBox", [10, 21, 0x40, 0x20, 0x0f])
        r1 = _root(st)
        _g(st, "showPopup.oddRepaint", [10, 21, 0x40, 0x20])
        return r1, _root(st)


def test_popup_odd_y_box_reads_the_colour_under_it(glib):
    r, rep = _odd(glib)
    assert r[21, 10] == 0x6318 and r[21 + 0x1f, 10 + 0x3f] == 0x6318            # the frame fill (left, y, w, h)
    assert (r[22:22 + 0x1e, 11:11 + 0x3e] == 0x1ce7).all()                       # (left + 1, y + 1, inner + 0x2f, h - 2)
    assert r[21 + 0x1f, 11] == 0x6318 and r[22, 11 + 0x3e] == 0x6318
    assert (rep[21:21 + 0x20, 10:10 + 0x40] == 0x1ce7).all() and rep[21, 9] == 0


def test_popup_odd_y_negctl_9_red(glib):
    assert not np.array_equal(_odd(glib)[0], _odd(glib, 9)[0])


@pytest.fixture(scope="module")
def tlib(glib):
    if not glib.has("gditext.attach") or not SG.INSTALL.is_dir():
        pytest.skip("no host text provider or no install")
    return glib


def _right(lib, mode=0):
    """drawStringAt2 and Widget_draw of `100` at x 300 in the target's quad colour; returns the two images"""
    with _negctl(lib, mode, REPORTS), _state(lib) as st:
        st.files_root(SG.INSTALL)
        st.call("gditext.attach", [])
        try:
            font = SG.DATA_VA + _offset(lib, "g_fontRef_821ee8")
            st.write(_offset(lib, "g_fontRef_821ee8") + 4, (1).to_bytes(4, "little"))
            raw = b"100\0"
            ctypes.memmove(ctypes.addressof(st._namebuf) + 0x200, raw, len(raw))
            _g(st, "Widget_setQuad", [ROOT, 0x80007fff, 0xffffffff, 2, 2])
            _g(st, "drawStringAt2", [ROOT, font, NAME_VA + 0x200, 300, 50, 3])
            _g(st, "Widget_draw", [ROOT, font, NAME_VA + 0x200, 300, 100, 3])
            return _root(st)
        finally:
            st.call("gditext.detach", [])


def test_draw_string_at2_ends_at_x_and_widget_draw_starts_at_x(tlib):
    r = _right(tlib)
    _ys, xs = np.nonzero(r[40:80] == 0x7fff)
    assert len(xs) > 5 and xs.max() < 300 and xs.max() >= 290
    _ys, xs2 = np.nonzero(r[90:130] == 0x7fff)
    assert len(xs2) > 5 and xs2.min() >= 300 and xs2.min() <= 305
    assert xs2.max() - xs2.min() == xs.max() - xs.min()   # the same run


def test_draw_string_at2_negctl_21_red(tlib):   # reports mode 21 (was dialogs mode 11)
    assert not np.array_equal(_right(tlib), _right(tlib, 21))


def main(argv=None) -> int:
    """print the gate's numbers per rectangle (and write them as TSV to argv[0] when given)"""
    lib = L.load()
    lines = ["scenario\tcheckpoint\trect\tpx\topaque\tfail\theld\theld_differ"]
    for scen in CASES:
        if not _have(scen):
            continue
        for c, rows in measure(lib, scen).items():
            for r in rows or [{"rect": "-", "px": -1, "opaque": -1, "fail": -1, "held": -1, "held_differ": -1}]:
                lines.append(f"{scen}\t{c}\t{r['rect']}\t{r['px']}\t{r['opaque']}\t{r['fail']}\t{r['held']}\t{r['held_differ']}")
                print(lines[-1], flush=True)
    if argv:
        open(argv[0], "w", encoding="utf-8").write("\n".join(lines) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
