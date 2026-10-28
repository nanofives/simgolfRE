"""Roadmap S3.6: body family `pixel` of the graphics backend (port/src/sim/gfx_pixel.cpp; notes log/roadmap/S3/6_notes.md).

Bodies: Widget_writePixel (the low word at (x, y), nothing outside the surface) and Widget_fillRect (jgld Surface::drawLine on a
16-bit surface: vline16 / hline16 with both ends drawn, a zero-length run draws nothing, Bresenham otherwise).

FRAMES: cov_a_keys k_render_dot / k_render_slash hold mainLoop's terrain-tool window (drawFrameR3 at (0x11a, 0x1a)) with its
Widget_fillRect separator at y 0x40, x 0x11a .. 0x21a, colour 0x0210; the port's row equals the original's back buffer there.
Control 2 (colour bit 0 flipped) reads RED on that row. Where S2.13's `reports` family is in the tree it owns Widget_fillRect
(the same jgld line16 plus Surface_box's clip, U-1278 / U-1356): the pixel family then registers Widget_writePixel only and the
fillRect controls use reports mode 5 (the line one pixel longer). Control 1 (the pixel one row lower) reads RED in the unit tests: no pinned
reference holds a Widget_writePixel decoration pixel the port's terrain-less frame can isolate (U-1355).

UNIT TESTS drive each body through gfx.call on the root surface.
"""
from __future__ import annotations

import ctypes

import numpy as np
import pytest

from port.test import gfx as GX
from port.test import lib as L
from port.test import segments as SG

pytestmark = pytest.mark.port

NAME_VA = 0x30000000
ROOT = 0x30000800            # the root surface's address the test gives f_stateptr_4c1570[0] (a key, never dereferenced)
W, H = 0x320, 0x258


@pytest.fixture(scope="module")
def plib():
    lib = L.load()
    if not lib.has("gfx.family.pixel"):
        pytest.skip("the library has no pixel family")
    if not lib.exe_loaded() and lib.exe_load():
        pytest.skip("no exe image (original/golf_clean.exe or SG_EXE_PATH)")
    return lib


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


def _log(st):
    st.call("gfx.log", [])
    return st.files_read("$gfx/log.txt").decode()


class _negctl:
    def __init__(self, lib, mode, name="gfx.family.pixel"):
        self.lib, self.mode, self.name = lib, mode, name

    def __enter__(self):
        self.prev = self.lib.negctl(self.name, self.mode)

    def __exit__(self, *e):
        self.lib.negctl(self.name, self.prev or 0)


def _draw(lib, calls, mode=0, name="gfx.family.pixel"):
    with _negctl(lib, mode, name), _state(lib) as st:
        base = _root(st)
        for name, args in calls:
            _g(st, name, [ROOT] + list(args))
        return base, _root(st), _log(st)


def _changed(base, img):
    ys, xs = np.nonzero(base != img)
    return {(int(x), int(y)): int(img[y, x]) for y, x in zip(ys, xs)}


def _fill_owner(lib):
    """(family, wrong-body mode) of the Widget_fillRect body in this library."""
    return ("gfx.family.reports", 5) if lib.has("gfx.family.reports") else ("gfx.family.pixel", 2)


def test_family_registered(plib):
    with _state(plib) as st:
        assert st.call("gfx.family.pixel", []).eax == (1 if plib.has("gfx.family.reports") else 2)
        assert st.call("gfx.check", []).eax == 0, st.files_read("$gfx/bodies.txt")


# ------------------------------------------------------------------------------------------------ Widget_writePixel
def test_write_pixel_writes_the_low_word(plib):
    base, img, log = _draw(plib, [("Widget_writePixel", (10, 20, 0x80001234))])
    assert _changed(base, img) == {(10, 20): 0x1234}
    assert "unsupported\tWidget_writePixel" not in log


@pytest.mark.parametrize("x,y", [(-1, 5), (5, -1), (W, 5), (5, H)])
def test_write_pixel_outside_writes_nothing(plib, x, y):
    base, img, _ = _draw(plib, [("Widget_writePixel", (x, y, 0x80007FFF))])
    assert _changed(base, img) == {}


def test_write_pixel_negctl_red(plib):
    base, img, _ = _draw(plib, [("Widget_writePixel", (10, 20, 0x80001234))], mode=1)
    assert _changed(base, img) != {(10, 20): 0x1234}


# ------------------------------------------------------------------------------------------------ Widget_fillRect
def test_fill_rect_hline_draws_both_ends(plib):
    base, img, _ = _draw(plib, [("Widget_fillRect", (30, 7, 20, 7, 0x80000210))])
    assert _changed(base, img) == {(x, 7): 0x210 for x in range(20, 31)}


def test_fill_rect_vline_draws_both_ends_clipped(plib):
    base, img, _ = _draw(plib, [("Widget_fillRect", (4, H - 3, 4, H + 9, 0x80000210))])
    assert _changed(base, img) == {(4, y): 0x210 for y in range(H - 3, H)}


def test_fill_rect_zero_length_draws_nothing(plib):
    base, img, _ = _draw(plib, [("Widget_fillRect", (9, 9, 9, 9, 0x80007FFF))])
    assert _changed(base, img) == {}


def test_fill_rect_two_pixel_mark(plib):
    # mainLoop's ball-shadow mark: (x, y) .. (x + 1, y) in 0x7fff
    base, img, _ = _draw(plib, [("Widget_fillRect", (100, 50, 101, 50, 0x80007FFF))])
    assert _changed(base, img) == {(100, 50): 0x7FFF, (101, 50): 0x7FFF}


def test_fill_rect_palette_index_is_unsupported(plib):
    base, img, log = _draw(plib, [("Widget_fillRect", (1, 1, 5, 1, 3))])
    assert _changed(base, img) == {}
    assert "unsupported\tWidget_fillRect: a palette colour index" in log


def test_fill_rect_diagonal_ends_and_one_pixel_per_column(plib):
    base, img, _ = _draw(plib, [("Widget_fillRect", (10, 10, 20, 14, 0x80000001))])
    ch = _changed(base, img)
    assert (10, 10) in ch and (20, 14) in ch
    assert sorted({x for x, _ in ch}) == list(range(10, 21)) and len(ch) == 11


def test_fill_rect_negctl_red(plib):
    fam, mode = _fill_owner(plib)
    base, img, _ = _draw(plib, [("Widget_fillRect", (30, 7, 20, 7, 0x80000210))], mode=mode, name=fam)
    assert _changed(base, img) != {(x, 7): 0x210 for x in range(20, 31)}


# ------------------------------------------------------------------------------------------------ frames
SCEN = "cov_a_keys"
CHK = ("k_render_dot", "k_render_slash")
ROW = (0x40, slice(0x11A, 0x21B))    # the separator 0x11a .. 0x21a
WIDE = (0x40, slice(0x11A, 0x21C))   # and the pixel after its right end (reports mode 5 draws it)


@pytest.fixture(scope="module")
def keys_run(plib):
    if not (GX.REFS / SCEN / "det_b" / f"{CHK[0]}.png").exists() or not SG.INSTALL.is_dir():
        pytest.skip("no pinned reference for cov_a_keys or no install")
    if not plib.has("gditext.attach"):
        pytest.skip("no host text provider (gditext: Windows GDI)")
    return GX.render(SCEN, list(CHK), plib)


def _row_bad(run, chk):
    img = run.frames.get(chk)
    assert img is not None, run.log
    ref = GX.reference(SCEN, chk)
    y, xs = ROW
    return int((img[y, xs] != ref[y, xs]).any(axis=1).sum())


@pytest.mark.parametrize("chk", CHK)
def test_tool_window_separator_equals_the_original(keys_run, chk):
    assert _row_bad(keys_run, chk) == 0


def test_tool_window_separator_negctl_red(plib, keys_run):
    fam, mode = _fill_owner(plib)
    run = GX.render(SCEN, list(CHK), plib, negctl=mode, negctl_name=fam)
    y, xs = WIDE
    for c in CHK:
        assert (run.frames[c][y, xs] != keys_run.frames[c][y, xs]).any(), (fam, mode, c)
    if fam == "gfx.family.pixel":
        assert all(_row_bad(run, c) > 0 for c in CHK)
