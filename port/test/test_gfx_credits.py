"""Roadmap S2.3: the credits screen (showCreditsScreen) and the surface / image lifecycle primitives it draws through.

Bodies (port/src/sim/gfx_surface.cpp, gfx_prim_text.cpp, gfx_credits.cpp; notes log/roadmap/S2/3_notes.md):
  surface    Image_ctor / Image_dtor / Image_releaseAll / Surface_ctor / Surface_free / Surface_createFull (the key's backend
             entries erased), Surface_blitTakeDirty (cropSprite through Surface_blit), Surface_drawRegion (jgld Sprite::draw ->
             draw16_1001cde0: only 0xff transparent, trims applied), Surface_blitThunk (Surface_blit3 with one shared size)
  prim_text  Widget_setQuad (the target's colour words), drawText (min(strlen, limit) characters in the main colour)
  credits    showCreditsScreen.logo / .lines / .line / .free (the logo at its own size, the kept lines, one line per call)
plus loadImage's flag bit 0 (gfx.cpp) and TraceInput::clock advancing snapshots (tick.cpp), both through 3_merge.md.

FRAMES: cov_r_quit's checkpoints quit_00 .. quit_03 and credits (the scroll at five positions) equal the original's back buffer
in every pixel. Controls RED there: prim_text 1 (black text), prim_text 2 (text one pixel right), credits 2 (line k drawn with
line k + 1's text), tick.frame 11 (clock reads advance no snapshot: every frame taken at the end of the input).
Blind on those frames, each judged by a unit test below: every surface mode (the frames never free and redraw an object, the
background crop has no trim to take, the logo line is not on any of the five frames), prim_text 3 (credits lines go through
draw_text_run, not the drawText body's limit), credits 1 and 3 (logo line / leading spaces: none on the frames). gfx.attach 3
(draw16's `> 0xfe`) stays blind: the background holds no byte 0xfe or 0xff (U-1200).

UNIT TESTS drive each body through gfx.call over synthetic PCX files in the files overlay and objects named by local keys.
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
LOCAL = 0xFFF00000           # gfx_local_key(0)
IMG, SPR, SPR2, LOGO = LOCAL + 0x5000, LOCAL + 0x5100, LOCAL + 0x5200, LOCAL + 0x5300


@pytest.fixture(scope="module")
def glib():
    lib = L.load()
    if not lib.has("gfx.family.credits"):
        pytest.skip("the library has no credits family")
    if not lib.exe_loaded() and lib.exe_load():
        pytest.skip("no exe image (original/golf_clean.exe or SG_EXE_PATH)")
    return lib


def _offset(lib, name):
    for n, off, _size in lib.objects():
        if n == name:
            return off
    raise KeyError(name)


def _state(lib, install=False):
    st = lib.state()
    st.set_data_base(SG.DATA_VA)
    st.fill_from_exe(True)
    if install:
        st.files_root(SG.INSTALL)
    st._namebuf = st.map(NAME_VA, bytearray(0x1000))
    st.write(_offset(lib, "f_stateptr_4c1570"), ROOT.to_bytes(4, "little"))
    st.call("gfx.attach", [])
    return st


def _put_str(st, off, s):
    raw = s.encode() + b"\0"
    ctypes.memmove(ctypes.addressof(st._namebuf) + off, raw, len(raw))
    return NAME_VA + off


def _g(st, name, args=()):
    _put_str(st, 0, name)
    return st.call("gfx.call", [NAME_VA] + [a & 0xFFFFFFFF for a in args] + [0] * (11 - len(args))).eax


def _root(st):
    assert st.call("gfx.dump", [1]).eax == 0
    return np.frombuffer(st.files_read("$gfx/root.555"), "<u2").reshape(H, W).copy()


def _log(st):
    st.call("gfx.log", [])
    return st.files_read("$gfx/log.txt").decode()


def _pcx(w, h, rows, pal):
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


def _w(r, g, b):
    return ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3)


PAL = {1: (0xF8, 0, 0), 2: (0, 0xF8, 0), 3: (0, 0, 0xF8), 0xFE: (0x80, 0x80, 0x80), 0xFF: (0xF8, 0xF8, 0xF8)}
# a 5 x 4 image: one 0xff row on top, one 0xff column on the left, a 0xfe pixel inside
ROWS = [[0xFF] * 5, [0xFF, 1, 2, 3, 1], [0xFF, 2, 0xFE, 1, 2], [0xFF, 3, 1, 2, 3]]


def _load(st, key=IMG, name="a.pcx", rows=ROWS, flags=1, pal=0):
    st.files_put(name, _pcx(len(rows[0]), len(rows), rows, PAL))
    return _g(st, "loadImage", [key, _put_str(st, 0x100, name), pal, 0, 0x100, flags])


def _negctl(lib, name, mode):
    class _Ctl:
        def __enter__(self):
            self.prev = lib.negctl(name, mode)

        def __exit__(self, *e):
            lib.negctl(name, self.prev or 0)
    return _Ctl()


# ------------------------------------------------------------------------------------------------ the family table
def test_families_registered_without_duplicates(glib):
    with _state(glib) as st:
        assert st.call("gfx.family.surface", []).eax == 9
        assert st.call("gfx.family.prim_text", []).eax == 2
        assert st.call("gfx.family.credits", []).eax == 4
        assert st.call("gfx.check", []).eax == 0, st.files_read("$gfx/bodies.txt")


# ------------------------------------------------------------------------------------------------ loadImage flag bit 0
def test_load_image_flag0_decodes_with_the_file_palette(glib):
    with _state(glib) as st:
        _load(st, flags=1)
        _g(st, "Surface_blit3", [IMG, ROOT, 1, 1, 4, 3, 10, 20, 4, 3])
        img, log = _root(st), _log(st)
    assert "unsupported\t" not in log, log
    want = [[_w(*PAL[v]) for v in r[1:]] for r in ROWS[1:]]
    assert img[20:23, 10:14].tolist() == want


def test_load_image_flag0_with_a_palette_argument_loads_nothing(glib):
    with _state(glib) as st:
        _load(st, flags=1, pal=5)
        _g(st, "Surface_blit3", [IMG, ROOT, 1, 1, 4, 3, 10, 20, 4, 3])
        img, log = _root(st), _log(st)
    assert not img.any()
    assert "Surface_blit3: a source or destination the backend does not model" in log


# ------------------------------------------------------------------------------------------------ lifecycle
# each op erases one key's entries: a sprite op leaves the image drawn and the sprite not, an image op the reverse
OPS = {"Surface_free": (SPR, 1), "Surface_ctor": (SPR, 8), "Image_releaseAll": (IMG, 2), "Image_dtor": (IMG, 2),
       "Image_ctor": (IMG, 8), "Surface_createFull": (IMG, 3)}


def _lifecycle(lib, op):
    with _state(lib) as st:
        _load(st)
        _g(st, "Surface_blitTakeDirty", [SPR, IMG, 0, 0, 5, 4, 1, 0])
        key, _mode = OPS[op]
        _g(st, op, [key, 0x320, 0x258, 0, 1, 0, 0] if op == "Surface_createFull" else [key])
        _g(st, "Image_draw", [SPR, ROOT, 10, 20, 0])
        _g(st, "Surface_blit3", [IMG, ROOT, 0, 0, 5, 4, 100, 20, 5, 4])
        img = _root(st)
    return img[20:24, 10:15].any(), img[20:24, 100:105].any()


@pytest.mark.parametrize("op", sorted(OPS))
def test_lifecycle_erases_the_key(glib, op):
    sprite_drawn, image_drawn = _lifecycle(glib, op)
    if OPS[op][0] == SPR:
        assert not sprite_drawn and image_drawn, op
    else:
        assert sprite_drawn and not image_drawn, op


@pytest.mark.parametrize("op", sorted(OPS))
def test_lifecycle_negctl_red(glib, op):
    with _negctl(glib, "gfx.family.surface", OPS[op][1]):
        sprite_drawn, image_drawn = _lifecycle(glib, op)
    assert sprite_drawn and image_drawn, op   # the op erased nothing


# ------------------------------------------------------------------------------------------------ crop, region draw, thunk
def _crop_and_draw(lib, trim, draw="Image_draw"):
    with _state(lib) as st:
        _load(st)
        _g(st, "Surface_blitTakeDirty", [SPR, IMG, 0, 0, 5, 4, 1, trim])
        _g(st, draw, [SPR, ROOT, 10, 20, 0])
        return _root(st), _log(st)


def test_blit_take_dirty_trims_and_draws_at_the_trim(glib):
    img, log = _crop_and_draw(glib, 1)
    assert "unsupported\t" not in log, log
    # kept rect 4 x 3 at (+1, +1): Image_draw's draw16 skips 0xfe and 0xff
    want = [[_w(*PAL[v]) if v < 0xFE else 0 for v in r[1:]] for r in ROWS[1:]]
    assert img[21:24, 11:15].tolist() == want
    assert int((img != 0).sum()) == 11


def test_blit_take_dirty_negctl_equivalent_on_a_plain_draw(glib):
    # trim off keeps the 0xff border, which every draw skips: the same pixels (the trim shows in the kept width: below)
    with _negctl(glib, "gfx.family.surface", 4):
        img, _ = _crop_and_draw(glib, 1)
    assert img[21:24, 11:15].tolist() == [[_w(*PAL[v]) if v < 0xFE else 0 for v in r[1:]] for r in ROWS[1:]]


def test_blit_take_dirty_negctl_red_on_the_logo_width(glib):
    # credits centre the logo by its kept width (+0x10): untrimmed it is 6, not 3
    with _negctl(glib, "gfx.family.surface", 4):
        img, _ = _credits(glib)
    assert not _logo_at(img, (0x320 - 3) // 2 + 1, 31)


def _region(lib, mode=0):
    with _negctl(lib, "gfx.family.surface", mode):
        return _crop_and_draw(lib, 1, "Surface_drawRegion")


def test_draw_region_draws_0xfe_and_skips_0xff_at_the_trims(glib):
    img, log = _region(glib)
    assert "unsupported\t" not in log, log
    want = [[_w(*PAL[v]) for v in r[1:]] for r in ROWS[1:]]   # no 0xff in the kept rect; 0xfe drawn (draw16_1001cde0)
    assert img[21:24, 11:15].tolist() == want
    assert int((img != 0).sum()) == 12


def test_draw_region_negctl_red_on_the_trims(glib):
    img, _ = _region(glib, 6)
    assert img[21:24, 11:15].tolist() != [[_w(*PAL[v]) for v in r[1:]] for r in ROWS[1:]]


def _untrimmed_region(lib, mode=0):
    with _negctl(lib, "gfx.family.surface", mode), _state(lib) as st:
        _load(st)
        _g(st, "Surface_blitTakeDirty", [SPR, IMG, 0, 0, 5, 4, 1, 0])
        _g(st, "Surface_drawRegion", [SPR, ROOT, 10, 20, 0])
        return _root(st)


def test_draw_region_with_the_untrimmed_0xff_border(glib):
    # trim 0 keeps the 0xff row / column: Sprite::draw leaves them transparent
    img = _untrimmed_region(glib)
    assert not img[20, 10:15].any() and not img[20:24, 10].any()
    assert int((img != 0).sum()) == 12


def test_draw_region_negctl_red_on_0xff(glib):
    img = _untrimmed_region(glib, 5)   # 0xff drawn as its palette word
    assert img[20, 10] == _w(*PAL[0xFF])


def test_draw_region_mask_is_unsupported(glib):
    with _state(glib) as st:
        _load(st)
        _g(st, "Surface_blitTakeDirty", [SPR, IMG, 0, 0, 5, 4, 1, 0])
        _g(st, "Surface_drawRegion", [SPR, ROOT, 10, 20, SPR2])
        assert "Surface_drawRegion: a mask or a destination other than the root surface" in _log(st)


def _thunk(lib, mode=0):
    with _negctl(lib, "gfx.family.surface", mode), _state(lib) as st:
        _load(st)
        _g(st, "Surface_blitThunk", [IMG, ROOT, 1, 2, 50, 60, 2, 2])
        return _root(st)


def test_blit_thunk_is_blit3_with_one_size(glib):
    img = _thunk(glib)
    assert img[60:62, 50:52].tolist() == [[_w(*PAL[v]) for v in r[1:3]] for r in ROWS[2:4]]
    assert int((img != 0).sum()) == 4


def test_blit_thunk_negctl_red(glib):
    img = _thunk(glib, 7)
    assert img[60:62, 50:52].tolist() != [[_w(*PAL[v]) for v in r[1:3]] for r in ROWS[2:4]]


# ------------------------------------------------------------------------------------------------ the credits bodies
LOGO_ROWS = [[0xFF] * 6, [0xFF, 1, 1, 2, 0xFF, 0xFF], [0xFF, 0xFE, 3, 2, 0xFF, 0xFF], [0xFF] * 6]   # kept 3 x 2 at (1, 1)
CREDITS = b"Intro\r\n#Credits  \r\nFirst line\r\n   $bink\r\n\r\n#EOF -- end\r\n"


def _credits(lib, mode=0, count=3, k=1, y=30):
    with _negctl(lib, "gfx.family.credits", mode), _state(lib) as st:
        _load(st, rows=LOGO_ROWS, name="logo.pcx")
        _g(st, "showCreditsScreen.logo", [LOGO, IMG])
        st.files_put("CREDITS.txt", CREDITS)
        _g(st, "showCreditsScreen.lines", [count])
        _g(st, "showCreditsScreen.line", [k, y, LOGO])
        return _root(st), _log(st)


def _logo_at(img, x, y):
    want = [[_w(*PAL[v]) for v in r[1:4]] for r in LOGO_ROWS[1:3]]
    return img[y:y + 2, x:x + 3].tolist() == want and int((img != 0).sum()) == 6


def test_credits_logo_line_is_centred_at_its_own_size(glib):
    img, log = _credits(glib)
    assert "unsupported\t" not in log, log
    assert _logo_at(img, (0x320 - 3) // 2 + 1, 31)


def test_credits_other_lines_draw_no_logo(glib):
    img, _ = _credits(glib, k=0)     # "First line": text (no font in this state: drawText draws nothing)
    assert not img.any()
    img, _ = _credits(glib, k=2)     # the empty line
    assert not img.any()


def test_credits_count_mismatch_is_reported(glib):
    img, log = _credits(glib, count=4)
    assert "showCreditsScreen.lines: the backend's line count differs from the screen's" in log
    assert not img.any()


def test_credits_free_drops_the_lines(glib):
    with _state(glib) as st:
        st.files_put("CREDITS.txt", CREDITS)
        _g(st, "showCreditsScreen.lines", [3])
        _g(st, "showCreditsScreen.free", [])
        _g(st, "showCreditsScreen.line", [1, 30, LOGO])
        assert "showCreditsScreen.line: a line the backend does not hold" in _log(st)


@pytest.mark.parametrize("mode,k", [(1, 1), (2, 0), (3, 1)])
def test_credits_negctl_red(glib, mode, k):
    img, _ = _credits(glib, mode=mode, k=k)
    good, _ = _credits(glib, k=k)
    assert img.tolist() != good.tolist(), mode


# ------------------------------------------------------------------------------------------------ text (host GDI provider)
@pytest.fixture(scope="module")
def tlib(glib):
    if not glib.has("gditext.attach") or not SG.INSTALL.is_dir():
        pytest.skip("no host text provider or no install")
    return glib


def _text(lib, text, limit, quad=(0x80007FFF, 0xFFFFFFFF, 2, 2), mode=0, x=50):
    with _negctl(lib, "gfx.family.prim_text", mode), _state(lib, install=True) as st:
        st.call("gditext.attach", [])
        try:
            font = SG.DATA_VA + _offset(lib, "g_fontRef_821020")
            st.write(_offset(lib, "g_fontRef_821020") + 4, (1).to_bytes(4, "little"))   # a font handle: drawText's +4 guard
            _g(st, "setDrawTarget", [ROOT, font, 0, 0, 0])
            if quad:
                _g(st, "Widget_setQuad", [ROOT] + list(quad))
            _g(st, "drawText", [ROOT, _put_str(st, 0x200, text), x, 100, limit])
            return _root(st), _log(st)
        finally:
            st.call("gditext.detach", [])


def test_draw_text_draws_the_limit_in_the_main_colour(tlib):
    img, log = _text(tlib, "Sid Meier", 3)
    assert "unsupported\t" not in log, log
    assert (img == _w(0xF8, 0xF8, 0xF8)).sum() > 20     # the main colour (the glyph edges are GDI's blend with the ground)
    assert not img[:, :50].any() and not img[:90].any()  # at the pen, below y
    short, _ = _text(tlib, "Sid", 99)
    assert img.tolist() == short.tolist()


def test_draw_text_without_a_quad_or_with_a_shadow_is_unsupported(tlib):
    img, log = _text(tlib, "Sid", 99, quad=None)
    assert "drawText: no Widget_setQuad seen for the target" in log and not img.any()
    img, log = _text(tlib, "Sid", 99, quad=(0x80007FFF, 1, 2, 2))
    assert "drawText: a shadow colour (not modelled)" in log and not img.any()


@pytest.mark.parametrize("mode", [1, 2, 3])
def test_draw_text_negctl_red(tlib, mode):
    good, _ = _text(tlib, "Sid Meier", 3)
    bad, _ = _text(tlib, "Sid Meier", 3, mode=mode)
    assert bad.tolist() != good.tolist(), mode


# ------------------------------------------------------------------------------------------------ the frames
SCEN = "cov_r_quit"
CHK = ("quit_00", "quit_01", "quit_02", "quit_03", "credits")
FRAME_NEGCTL = [("gfx.family.prim_text", 1, "black text"), ("gfx.family.prim_text", 2, "text one pixel right"),
                ("gfx.family.credits", 2, "line k drawn with line k + 1's text"),
                ("tick.frame", 11, "clock reads advance no snapshot")]


@pytest.fixture(scope="module")
def credits_run(glib):
    if not (GX.REFS / SCEN / "det_b" / "credits.png").exists() or not SG.INSTALL.is_dir():
        pytest.skip("no pinned reference for cov_r_quit or no install")
    if not glib.has("gditext.attach"):
        pytest.skip("no host text provider (gditext: Windows GDI)")
    return GX.render(SCEN, list(CHK), glib)


@pytest.mark.parametrize("chk", CHK)
def test_credits_frames_equal_the_original(credits_run, chk):
    assert credits_run.frames.get(chk) is not None, credits_run.log
    r = GX.compare(credits_run, chk)
    assert r["total"] == 0, (chk, r)


def test_credits_bodies_handled(credits_run):
    for name in ("Surface_ctor", "Image_ctor", "Surface_createFull", "loadImage", "Surface_blitTakeDirty",
                 "showCreditsScreen.logo", "Image_releaseAll", "showCreditsScreen.lines", "setDrawTarget", "Widget_setQuad",
                 "Image_draw", "showCreditsScreen.line", "Window::refreshFull"):
        assert f"handled\t{name}\t" in credits_run.log, name
    for bad in ("loadImage: flag bit 0", "showCreditsScreen", "Surface_drawRegion", "drawText"):
        assert not any(l.startswith("unsupported\t" + bad) for l in credits_run.log.splitlines()), bad


@pytest.mark.parametrize("name,mode,what", FRAME_NEGCTL)
def test_credits_frames_negctl_red(glib, credits_run, name, mode, what):
    run = GX.render(SCEN, list(CHK), glib, negctl=mode, negctl_name=name)
    assert sum(GX.compare(run, c)["total"] != 0 for c in CHK) > 0, (name, mode, what)
