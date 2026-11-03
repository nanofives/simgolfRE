"""Roadmap S2.4: the routing map (showRoutingMapScreen) and the load browser (loadGameBrowserScreen), with the fill / panel /
shadowed-text primitives they draw through.

Bodies (notes log/roadmap/S2/4_notes.md):
  prim_fill   fillWindowRect / Win_fillRect (jgld Surface::fillRect16 with a direct colour, clipped), blitPanel (style 0 the
              plain tile, else the nine-cell panel, through Sprite::drawG), Display::setScale (the blit scale; Image_draw
              reports any scale but (1, 1, 1)), gfxf::fill_convex_poly16 (fillConvexPoly16 with its edge walker and span filler)
  prim_text2  drawCenteredText2 (a shadow run in run-palette colour 1 at (+0, +1), then the main run, centred)
  routing     loadInfoScreenArt.images (the report art manifest), drawRoutingPoly.diamond (the map's tile)
plus the screens' drawing calls in routing.cpp (MapDraw) and saveload.cpp (the browser), the boot's loadInfoScreenArt hook-up
(tick_boot.inc) and Image_draw's scale test (gfx.cpp), all through 4_merge.md.

FRAMES: cov_k1_routing `routing` and cov_a_keys `k_dlg_L` equal the original's back buffer in every pixel. cov_a_keys `k_dlg_R`
(two holes built) differs only inside the map's hole overlay: the white tee-to-green lines (Terrain::drawLine, OpenGL: S3.8, not
drawn without LiveTerrain's GL context in this run); the test pins that bound. The tee and flag sprites are drawn since roadmap
S3.17 (addSprite of the FLIC sheet cells, test_gfx_s3_17.py). Controls RED on the frames: prim_fill 1 / 3, routing 1 / 2 / 3 / 4, showRoutingMapScreen 10, loadGameBrowserScreen
9, prim_text2 1 / 2 (the k_dlg_R labels). Judged by unit tests instead: prim_fill 2 (style 0 tiles: the browser's tip, no frame),
prim_fill 4 (the scale: no Image_draw runs at another scale on the frames).
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
WIN = ROOT - 0x274           # a window whose Surface part (+0x274) is the root surface
W, H = 0x320, 0x258
LOCAL = 0xFFF00000           # gfx_local_key(0)
IMG, SPR = LOCAL + 0x6000, LOCAL + 0x6100


@pytest.fixture(scope="module")
def glib():
    lib = L.load()
    if not lib.has("gfx.family.routing"):
        pytest.skip("the library has no routing family")
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


def _negctl(lib, name, mode):
    class _Ctl:
        def __enter__(self):
            self.prev = lib.negctl(name, mode)

        def __exit__(self, *e):
            lib.negctl(name, self.prev or 0)
    return _Ctl()


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


# ------------------------------------------------------------------------------------------------ the family table
def test_families_registered_without_duplicates(glib):
    with _state(glib) as st:
        assert st.call("gfx.family.prim_fill", []).eax == 4
        assert st.call("gfx.family.prim_text2", []).eax == 1
        assert st.call("gfx.family.routing", []).eax == 2
        assert st.call("gfx.check", []).eax == 0, st.files_read("$gfx/bodies.txt")


# ------------------------------------------------------------------------------------------------ fills
def _fill(lib, name, target, rect, colour, mode=0):
    with _negctl(lib, "gfx.family.prim_fill", mode), _state(lib) as st:
        _g(st, name, [target] + list(rect) + [colour])
        return _root(st), _log(st)


def test_fill_window_rect_fills_the_rectangle_with_the_low_word(glib):
    img, log = _fill(glib, "fillWindowRect", WIN, (10, 20, 30, 5), 0x800023E8)
    assert "unsupported\t" not in log, log
    exp = np.zeros((H, W), np.uint16)
    exp[20:25, 10:40] = 0x23E8
    assert img.tolist() == exp.tolist()


def test_win_fill_rect_is_clipped_to_the_surface(glib):
    img, _ = _fill(glib, "Win_fillRect", ROOT, (W - 4, -3, 10, 6), 0x80007FFF)
    exp = np.zeros((H, W), np.uint16)
    exp[0:3, W - 4:W] = 0x7FFF
    assert img.tolist() == exp.tolist()
    img, _ = _fill(glib, "Win_fillRect", ROOT, (5, 5, -3, 4), 0x80007FFF)   # an empty rectangle fills nothing
    assert not img.any()


def test_fill_with_a_palette_index_or_another_surface_is_unsupported(glib):
    img, log = _fill(glib, "fillWindowRect", WIN, (10, 20, 30, 5), 0x17)
    assert "fillWindowRect: a palette colour index" in log and not img.any()
    img, log = _fill(glib, "Win_fillRect", ROOT + 4, (10, 20, 30, 5), 0x80001234)
    assert "Win_fillRect: a surface other than the root window's" in log and not img.any()


def test_fill_negctl_red(glib):
    good, _ = _fill(glib, "fillWindowRect", WIN, (10, 20, 30, 5), 0x800023E8)
    bad, _ = _fill(glib, "fillWindowRect", WIN, (10, 20, 30, 5), 0x800023E8, mode=1)
    assert bad.tolist() != good.tolist()


# ------------------------------------------------------------------------------------------------ the convex polygon fill
def _poly_model(pts, w=W, h=H):
    """fillConvexPoly16 (c3db.cpp scanPolygon, c3x.cpp polyEdgeStep, c3t.cpp polyEdgeAdvance / fillSpanDown), written again
    from those transcriptions: the set of (x, y) pixels it writes on a w x h surface whose clip is the whole surface."""
    n = len(pts)
    ys = [p[1] for p in pts]
    min_y, max_y = 0x7FFF, -0x7FFD
    start = end = 0
    for i, y in enumerate(ys):
        if y < min_y:
            min_y, start = y, i
        if y > max_y:
            max_y, end = y, i
    if min_y >= max_y:
        return set()

    def tdiv(a, b):
        q = abs(a) // abs(b)
        return q if (a >= 0) == (b >= 0) else -q

    def step(e, idx):
        if idx == end:
            return False
        while True:
            v = idx + e["dir"]
            v = 0 if v > n - 1 else (n - 1 if v < 0 else v)
            dy = pts[v][1] - pts[idx][1]
            e["rows"] = dy
            if dy == 0:
                idx = v
                if v == end:
                    return False
                continue
            if dy < 0:
                return False
            e["end"], e["x"] = v, pts[idx][0]
            dx = pts[v][0] - pts[idx][0]
            if dx >= 0:
                e.update(sign=1, step=tdiv(dx, dy), err=0, es=dx - tdiv(dx, dy) * dy)
            else:
                a = -dx
                e.update(sign=-1, err=1 - dy, step=-tdiv(a, dy), es=a - tdiv(a, dy) * dy)
            e["reset"] = dy
            return True

    def adv(e):
        e["rows"] -= 1
        if e["rows"] == 0:
            return step(e, e["end"])
        e["x"] += e["step"]
        e["err"] += e["es"]
        if e["err"] > 0:
            e["x"] += e["sign"]
            e["err"] -= e["reset"]
        return True

    out = set()
    e1, e2 = {"dir": -1}, {"dir": 1}
    if not step(e1, start) or not step(e2, start):
        return out
    row = min_y
    if row >= h - 1:
        return out
    while True:
        if row >= 0:
            x0, x1 = sorted((e1["x"], e2["x"]))
            if not (x1 <= 0 or x0 >= w - 1 or x1 <= x0):
                x1, x0 = min(x1, w - 1), max(x0, 0)
                out.update((x, row) for x in range(x0, x1))
        if not adv(e1) or not adv(e2):
            return out
        row += 1
        if row >= h - 1:
            return out


@pytest.mark.parametrize("cx,cy", [(100, 100), (W - 3, 50), (2, H - 2), (400, 0)])
def test_diamond_equals_the_scan_model(glib, cx, cy):
    with _state(glib) as st:
        _g(st, "drawRoutingPoly.diamond", [cx, cy, 0x80001284])
        img = _root(st)
    got = {(x, y) for y, x in zip(*np.nonzero(img))}
    exp = _poly_model([(cx - 6, cy), (cx, cy - 3), (cx + 6, cy), (cx, cy + 3)])
    assert got == exp
    assert all(img[y, x] == 0x1284 for x, y in got)


@pytest.mark.parametrize("mode", [3])
def test_diamond_negctl_red(glib, mode):
    with _negctl(glib, "gfx.family.prim_fill", mode), _state(glib) as st:
        _g(st, "drawRoutingPoly.diamond", [100, 100, 0x80001284])
        bad = _root(st)
    with _state(glib) as st:
        _g(st, "drawRoutingPoly.diamond", [100, 100, 0x80001284])
        good = _root(st)
    assert bad.tolist() != good.tolist()


def test_diamond_palette_index_is_unsupported(glib):
    with _state(glib) as st:
        _g(st, "drawRoutingPoly.diamond", [100, 100, 0x12])
        assert "drawRoutingPoly: a palette colour index" in _log(st) and not _root(st).any()


# ------------------------------------------------------------------------------------------------ blitPanel
@pytest.fixture(scope="module")
def ilib(glib):
    if not SG.INSTALL.is_dir():
        pytest.skip("no install")
    return glib


def _panel(lib, style, mode=0, art=True):
    with _negctl(lib, "gfx.family.prim_fill", mode), _state(lib, install=True) as st:
        if art:
            _g(st, "loadInterfaceArt.images")
        _g(st, "Win_fillRect", [ROOT, 90, 90, 80, 40, 0x80007FFF])   # a white ground: style 0's tile shades what is under it
        _g(st, "blitPanel", [100, 100, 0x30, 0x10, style])
        return _root(st), _log(st)


def test_blit_panel_style_0_tiles_the_plain_cell(ilib):
    img, log = _panel(ilib, 0)
    assert "blitPanel" not in log.replace("handled\tblitPanel", ""), log
    assert (img[100:116, 100:148] != 0x7FFF).all()                       # every pixel of the three tiles shaded
    assert (img[90:100, 90:170] == 0x7FFF).all() and (img[116:130, 90:170] == 0x7FFF).all()   # nothing outside
    # every 16 x 16 tile of style 0 is the same cell: the three columns are equal
    assert img[100:116, 100:116].tolist() == img[100:116, 116:132].tolist() == img[100:116, 132:148].tolist()


def test_blit_panel_style_1_takes_the_nine_cells(ilib):
    a, _ = _panel(ilib, 0)
    b, _ = _panel(ilib, 1)
    assert a.tolist() != b.tolist()


def test_blit_panel_negctl_2_red(ilib):
    good, _ = _panel(ilib, 0)
    bad, _ = _panel(ilib, 0, mode=2)
    assert bad.tolist() != good.tolist()


def test_blit_panel_without_tiles_is_unsupported(glib):
    img, log = _panel(glib, 0, art=False)
    assert "blitPanel: a tile the backend has no sprite for" in log and (img[90:130, 90:170] == 0x7FFF).all()


# ------------------------------------------------------------------------------------------------ Display::setScale
def _scaled_draw(lib, scale, mode=0):
    with _negctl(lib, "gfx.family.prim_fill", mode), _state(lib) as st:
        st.files_put("a.pcx", _pcx(4, 2, [[1, 1, 1, 1], [1, 1, 1, 1]], {1: (0xF8, 0, 0)}))
        _g(st, "loadImage", [IMG, _put_str(st, 0x100, "a.pcx"), 0, 0, 0x100, 2])
        _g(st, "Surface_blit", [SPR, IMG, 0, 0, 4, 2, 1, 0])
        _g(st, "Display::setScale", list(scale))
        _g(st, "Image_draw", [SPR, ROOT, 10, 10, 0])
        return _root(st), _log(st)


def test_image_draw_at_scale_1_draws(glib):
    img, log = _scaled_draw(glib, (1, 1, 1))
    assert img[10:12, 10:14].tolist() == [[0x7C00] * 4] * 2, log


def test_image_draw_at_another_scale_is_drawn_by_the_scene_family(glib):
    # S3.5: a scale other than (1, 1, 1) is drawB under that scale (gfxs::image_draw), no longer reported; at (1, 1, 5) the
    # 4 x 2 image scales to (int)(4 * 0.2) = 0 columns, so the magnifying (2, 2, 1) is the one that leaves pixels
    img, log = _scaled_draw(glib, (2, 2, 1))
    assert "Image_draw: a display scale other than (1, 1, 1)" not in log and img.any()


def test_set_scale_negctl_4_red(glib):
    good, _ = _scaled_draw(glib, (1, 1, 5))
    bad, _ = _scaled_draw(glib, (1, 1, 5), mode=4)
    assert bad.tolist() != good.tolist()


# ------------------------------------------------------------------------------------------------ drawCenteredText2
@pytest.fixture(scope="module")
def tlib(glib):
    if not glib.has("gditext.attach") or not SG.INSTALL.is_dir():
        pytest.skip("no host text provider or no install")
    return glib


def _text2(lib, text, mode=0, colour=0x80007FF0):
    with _negctl(lib, "gfx.family.prim_text2", mode), _state(lib, install=True) as st:
        st.call("gditext.attach", [])
        try:
            font = SG.DATA_VA + _offset(lib, "g_fontRef_821ee8")
            st.write(_offset(lib, "g_fontRef_821ee8") + 4, (1).to_bytes(4, "little"))   # a font handle: drawText's +4 guard
            _g(st, "setDrawTarget", [ROOT, font, 0, 0, 0])
            _g(st, "drawCenteredText2", [_put_str(st, 0x200, text), 200, 100, colour])
            return _root(st), _log(st)
        finally:
            st.call("gditext.detach", [])


SHADOW = 0x10 << 10   # RGB555 (16, 0, 0): the run palette's entry 1 (U-1230)


def test_centered_text2_draws_a_shadow_below_the_main_run(tlib):
    img, log = _text2(tlib, "12")
    assert "unsupported\t" not in log, log
    main = img == 0x7FF0
    assert main.sum() > 5 and (img == SHADOW).sum() > 2
    ys, xs = np.nonzero(img == SHADOW)
    my, _ = np.nonzero(main)
    assert ys.max() == my.max() + 1          # the shadow run is one row lower (offset (0, 1))
    assert abs(xs.mean() - 200) < 6           # centred on x


@pytest.mark.parametrize("mode", [1, 2])
def test_centered_text2_negctl_red(tlib, mode):
    good, _ = _text2(tlib, "12")
    bad, _ = _text2(tlib, "12", mode=mode)
    assert bad.tolist() != good.tolist()


# ------------------------------------------------------------------------------------------------ the frames
ROUTE = ("cov_k1_routing", ["routing"])
KEYS = ("cov_a_keys", ["k_dlg_R", "k_dlg_L"])
K_DLG_R_BOUND = 246   # the hole overlay's GL lines (S3.8: no Terrain GL context in this run); the sprites are drawn (S3.17)


def _have(scen, chk):
    return (GX.REFS / scen / "det_b" / f"{chk}.png").exists() and SG.INSTALL.is_dir()


@pytest.fixture(scope="module")
def route_run(glib):
    if not _have(ROUTE[0], "routing") or not glib.has("gditext.attach"):
        pytest.skip("no pinned reference / install / host text provider")
    return GX.render(ROUTE[0], ROUTE[1], glib)


@pytest.fixture(scope="module")
def keys_run(glib):
    if not _have(KEYS[0], "k_dlg_R") or not glib.has("gditext.attach"):
        pytest.skip("no pinned reference / install / host text provider")
    return GX.render(KEYS[0], KEYS[1], glib)


def test_routing_map_frame_equals_the_original(route_run):
    r = GX.compare(route_run, "routing")
    assert r["total"] == 0, r


def test_load_browser_frame_equals_the_original(keys_run):
    r = GX.compare(keys_run, "k_dlg_L")
    assert r["total"] == 0, r


def test_routing_map_with_holes_differs_only_in_the_hole_overlay(keys_run):
    r = GX.compare(keys_run, "k_dlg_R")
    assert 0 <= r["total"] <= K_DLG_R_BOUND, r
    x0, y0, x1, y1 = r["bbox"]
    assert 0x184 <= x0 and x1 <= 0x1e0 and 0x160 <= y0 and y1 <= 0x1a0, r   # the two holes' map area
    assert "no body: showRoutingMapScreen" not in keys_run.log   # roadmap S3.17: the tee / flag sprites through addSprite
    assert "handled	addSprite	" in keys_run.log
    assert "handled\tlineTo\t" in keys_run.log   # roadmap S3.8: the hole line through setLinePoint / lineTo


def test_screens_bodies_handled(route_run, keys_run):
    for name in ("loadInfoScreenArt.images", "drawRoutingPoly.diamond", "Display::setScale", "fillWindowRect", "Image_draw",
                 "drawCenteredText", "Window::refreshFull"):
        assert f"handled\t{name}\t" in route_run.log, name
    for name in ("Surface_blitThunk", "Surface_blit", "loadImage", "drawTextWithColor", "drawCenteredText2"):
        assert f"handled\t{name}\t" in keys_run.log, name
    for bad in ("fillWindowRect:", "drawRoutingPoly", "Image_draw: a display scale", "loadInfoScreenArt"):
        assert not any(l.startswith("unsupported\t" + bad) for l in route_run.log.splitlines()), bad


FRAME_NEGCTL = [
    ("gfx.family.prim_fill", 1, ROUTE, "routing", "the fill one row short"),
    ("gfx.family.prim_fill", 3, ROUTE, "routing", "polygon spans one pixel longer"),
    ("gfx.family.routing", 1, ROUTE, "routing", "route buttons cut from the next table entry"),
    ("gfx.family.routing", 2, ROUTE, "routing", "the diamond one pixel wider"),
    ("gfx.family.routing", 3, ROUTE, "routing", "route screens one row short"),
    ("gfx.family.routing", 4, ROUTE, "routing", "no report art"),
    ("showRoutingMapScreen", 10, ROUTE, "routing", "the selection highlight one row lower"),
    ("loadGameBrowserScreen", 9, KEYS, "k_dlg_L", "file names one pixel right"),
]


@pytest.mark.parametrize("name,mode,scen,chk,what", FRAME_NEGCTL)
def test_frames_negctl_red(glib, route_run, name, mode, scen, chk, what):
    run = GX.render(scen[0], scen[1], glib, negctl=mode, negctl_name=name)
    assert GX.compare(run, chk)["total"] != 0, (name, mode, what)


@pytest.mark.parametrize("mode", [1, 2])
def test_k_dlg_r_labels_negctl_red(glib, keys_run, mode):
    run = GX.render(KEYS[0], ["k_dlg_R"], glib, negctl=mode, negctl_name="gfx.family.prim_text2")
    assert GX.compare(run, "k_dlg_R")["total"] > GX.compare(keys_run, "k_dlg_R")["total"], mode
