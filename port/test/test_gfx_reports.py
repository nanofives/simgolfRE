"""Roadmap S2.8: the report screens over the course (F2-F9 reports, hole stats, best scores, end of year, the hire dialog and
preferences) on the pinned scenario cov_r_s2_screens.

Bodies: family `reports` (port/src/sim/gfx_reports.cpp: Surface_drawSprite through drawAlpha's quarter blend, Widget_draw /
Widget_draw2 / drawStringAt / drawStringAt2, Surface_box / Surface::callBounds, the screen bodies) and the screens' drawing calls in
their port functions (gfx_reports.h ScreenDraw; notes log/roadmap/S2/8_notes.md).

THE JUDGED PIXELS. Every one of these screens is drawn over the in-game frame (the course, the HUD and the toolbar), which the
backend does not draw yet (roadmap S3), and every one first dims the whole frame (black.pcx at alpha 0.25). A checkpoint is
therefore judged on the pixels the screen determines: the boot runs three more times with a different underlay (negctl modes
101..103 of the reports family fill the root surface with 0x0000, 0x7fff, 0x2a55 at the screen's first call, `reports.begin`),
and a pixel whose port value changes with the underlay depends on what lay beneath (held for S3: the dimmed course, the art's
blended edges). The others must equal the original's back buffer exactly; their number is pinned (JUDGED), so a screen that
stops drawing something (its pixels would turn into held ones) reads RED too.
"""
from __future__ import annotations

import argparse
import json
import pathlib
import sys

import numpy as np
import pytest

from port.test import gfx as GX
from port.test import lib as L

pytestmark = pytest.mark.port

SCENARIO = "cov_r_s2_screens"
FAMILY = "gfx.family.reports"
UNDERLAYS = (101, 102, 103)
CHECKPOINTS = ["rep_comments", "rep_histograph", "rep_financial", "rep_report1", "rep_shortcuts", "rep_roster", "hole_stats",
               "best_scores", "end_of_year", "preferences", "hire"]
# the checkpoints this step judges (rep_report1 is showCourseReport's grading page and preferences showPopup's check list:
# roadmap S2.9's functions, judged here once its code is in the tree)
JUDGED_CHECKPOINTS = ["rep_comments", "rep_histograph", "rep_financial", "rep_shortcuts", "rep_roster", "hole_stats",
                      "best_scores", "end_of_year", "hire"]
# checkpoint -> the number of pixels the screen determines (not held), pinned from a run whose judged pixels all equal the
# original's
JUDGED = {"rep_comments": 53893, "rep_histograph": 411234, "rep_financial": 195269, "rep_shortcuts": 266940, "rep_roster": 434887,
          "hole_stats": 100373, "best_scores": 45359, "end_of_year": 92616, "hire": 148198}
# judged pixels that still differ, all of them the hire dialog's four animated employees: addSprite draws a FLIC sheet cell
# through drawQueuedSprite (roadmap S3's sprites; reported unsupported, U-1272): checkpoint -> (count, bbox x0, y0, x1, y1)
RESIDUAL = {"hire": (1397, (556, 167, 586, 403))}
# frame controls of the reports family (log/roadmap/S2/8_controls.tsv): mode -> the checkpoints it changes. Modes 1 (the
# quarter blend halved), 5 (a line one pixel longer) and 7 (Surface_box without effect) change no judged pixel on these frames
# (the dimming lies under held pixels only, no histograph line crosses the box on this early date): unit tests below.
FRAME_CONTROLS = {2: ["rep_financial", "end_of_year"], 3: ["rep_comments", "rep_histograph", "rep_financial", "rep_shortcuts",
                                                         "rep_roster", "hole_stats", "best_scores", "end_of_year"],
                  4: ["rep_comments", "rep_financial", "rep_shortcuts", "rep_roster", "hole_stats", "best_scores"],
                  6: ["rep_roster"], 8: ["rep_financial", "rep_shortcuts", "rep_roster", "best_scores", "end_of_year"],
                  11: ["rep_financial"], 12: ["rep_comments"], 13: ["best_scores"], 14: ["hole_stats"], 15: ["rep_roster"],
                  16: ["rep_shortcuts"], 17: ["end_of_year"], 18: ["hire"], 19: ["rep_histograph"]}

# (roadmap S2.11: control 8 no longer lists hole_stats. There it changed 8 judged pixels, one-level differences of the hole-stats
# panel's blended border over the roster screen the port left in the root; since the bulletin board is drawn, the root under
# the panel is the board, which the control does not change: log/roadmap/S2/11_ctl8.log, 11_notes.md section 5.)


def held_masks(checkpoints, lib=None) -> dict:
    """checkpoint -> bool array (H, W): True where the port's frame changes with the underlay"""
    runs = [GX.render(SCENARIO, checkpoints, lib, negctl=m, negctl_name=FAMILY) for m in UNDERLAYS]
    out = {}
    for c in checkpoints:
        imgs = [r.frames.get(c) for r in runs]
        if any(i is None for i in imgs):
            out[c] = None
            continue
        h = np.zeros(imgs[0].shape[:2], bool)
        for i in imgs[1:]:
            h |= (i != imgs[0]).any(axis=2)
        out[c] = h
    return out


def judge(checkpoints, lib=None, negctl: tuple[str, int] | None = None, held: dict | None = None) -> dict:
    """checkpoint -> {judged, diff, bbox, unsupported_before}"""
    lib = lib or L.load()
    held = held if held is not None else held_masks(checkpoints, lib)
    if negctl:
        run = GX.render(SCENARIO, checkpoints, lib, negctl=negctl[1], negctl_name=negctl[0])
    else:
        run = GX.render(SCENARIO, checkpoints, lib)
    out = {}
    for c in checkpoints:
        img, h = run.frames.get(c), held.get(c)
        if img is None or h is None:
            out[c] = {"judged": -1, "diff": -1, "bbox": None, "why": "no snapshot"}
            continue
        ref = GX.reference(SCENARIO, c)
        bad = (img != ref).any(axis=2) & ~h
        ys, xs = np.nonzero(bad)
        out[c] = {"judged": int((~h).sum()), "diff": int(bad.sum()),
                  "bbox": [int(xs.min()), int(ys.min()), int(xs.max()), int(ys.max())] if len(xs) else None,
                  "unsupported_before": run.unsupported_before.get(c)}
    out["$log"] = run.log
    return out


# ------------------------------------------------------------------------------------------------ tests
@pytest.fixture(scope="module")
def rlib():
    lib = L.load()
    if not lib.has(FAMILY):
        pytest.skip("the library has no reports family")
    if not lib.exe_loaded() and lib.exe_load():
        pytest.skip("no exe image (original/golf_clean.exe or SG_EXE_PATH)")
    return lib


@pytest.fixture(scope="module")
def held(rlib):
    if not (GX.REFS / SCENARIO / "det_b" / "summary.json").exists():
        pytest.skip("no pinned reference of cov_r_s2_screens")
    return held_masks(JUDGED_CHECKPOINTS, rlib)


@pytest.fixture(scope="module")
def judged(rlib, held):
    return judge(JUDGED_CHECKPOINTS, rlib, held=held)


@pytest.mark.parametrize("c", JUDGED_CHECKPOINTS)
def test_frame_judged_pixels_equal_the_original(judged, c):
    r = judged[c]
    assert r["judged"] == JUDGED[c], r
    if c in RESIDUAL:
        n, (x0, y0, x1, y1) = RESIDUAL[c]
        assert r["diff"] == n, r
        bx = r["bbox"]
        assert x0 <= bx[0] and y0 <= bx[1] and bx[2] <= x1 and bx[3] <= y1, r
    else:
        assert r["diff"] == 0, r


def test_frames_report_the_unmodelled_sprites(judged):
    assert "no body: addSprite" in judged["$log"]


@pytest.mark.parametrize("mode", [3, 12])
def test_frame_controls_red(rlib, held, judged, mode):
    _control(rlib, held, judged, mode)


@pytest.mark.slow
@pytest.mark.parametrize("mode", sorted(set(FRAME_CONTROLS) - {3, 12}))
def test_frame_controls_red_all(rlib, held, judged, mode):
    _control(rlib, held, judged, mode)


def _control(rlib, held, judged, mode):
    r = judge(JUDGED_CHECKPOINTS, rlib, negctl=(FAMILY, mode), held=held)
    for c in FRAME_CONTROLS[mode]:
        assert r[c]["diff"] != judged[c]["diff"] or r[c]["judged"] != judged[c]["judged"], (mode, c, r[c])


# ------------------------------------------------------------------------------------------------ unit tests of the bodies
NAME_VA = 0x30000000
ROOT = 0x30000800            # f_stateptr_4c1570[0] (a key the backend compares, never dereferenced)
W, H = 0x320, 0x258
LOCAL = 0xFFF00000           # gfx_local_key(0)
IMG, SPR = LOCAL + 0x7000, LOCAL + 0x7100


def _offset(lib, name):
    for n, off, _size in lib.objects():
        if n == name:
            return off
    raise KeyError(name)


def _state(lib):
    from port.test import segments as SG
    st = lib.state()
    st.set_data_base(SG.DATA_VA)
    st.fill_from_exe(True)
    st._namebuf = st.map(NAME_VA, bytearray(0x1000))
    st.write(_offset(lib, "f_stateptr_4c1570"), ROOT.to_bytes(4, "little"))
    st.call("gfx.attach", [])
    return st


def _put_str(st, off, s):
    import ctypes
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


def _w555(r, g, b):
    return ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3)


class _Neg:
    def __init__(self, lib, mode):
        self.lib, self.mode = lib, mode

    def __enter__(self):
        self.prev = self.lib.negctl(FAMILY, self.mode)

    def __exit__(self, *e):
        self.lib.negctl(FAMILY, self.prev or 0)


PAL = {1: (0xF8, 0, 0), 2: (0, 0xF8, 0), 3: (0x40, 0x80, 0xC0), 0xF8: (0x10, 0x10, 0x10), 0xFF: (0xF8, 0xF8, 0xF8)}
ROWS = [[1, 2, 3, 0xF8], [0xFF, 3, 2, 1], [2, 0xF8, 0xFF, 3]]


def _sprite(st):
    st.files_put("q.pcx", _pcx(4, 3, ROWS, PAL))
    assert _g(st, "loadImage", [IMG, _put_str(st, 0x100, "q.pcx"), 0, 0, 0x100, 1]) == 1
    _g(st, "Surface_blit", [SPR, IMG, 0, 0, 4, 3, 1, 0])   # untrimmed


def _quarter(p, d):
    """draw16_1002ed60's loop body (re/match/jgld_blit16.cpp), written from its instructions: both words shifted left 4, red +
    blue under 0x7c1f0 and green under 0x3e00 summed, halved, the destination's group added, halved, masked, summed, >> 4"""
    P, D = p << 4, d << 4
    e = (((P & 0x7c1f0) + (D & 0x7c1f0)) >> 1)
    f = (((P & 0x3e00) + (D & 0x3e00)) >> 1)
    e = (e + (D & 0x7c1f0)) >> 1
    f = (f + (D & 0x3e00)) >> 1
    return (((e & 0x7c1f0) + (f & 0x3e00)) >> 4) & 0xffff


def _alpha(v):
    import struct
    return struct.unpack("<I", struct.pack("<f", v))[0]


def _underlay(lib, st, mode):
    with _Neg(lib, mode):
        _g(st, "reports.begin", [])


@pytest.mark.parametrize("flag", [0, 1])
def test_draw_sprite_quarter_blend_equals_the_blitter(rlib, flag):
    with _state(rlib) as st:
        _sprite(st)
        _underlay(rlib, st, 103)   # 0x2a55 everywhere
        assert _g(st, "Surface_drawSprite", [SPR, ROOT, 10, 20, _alpha(0.25), 0, flag]) == 1
        img = _root(st)
        pal = {k: _w555(*v) for k, v in PAL.items()}
        for r, row in enumerate(ROWS):
            for c, b in enumerate(row):
                want = 0x2a55 if b >= 0xff or (flag and b >= 0xf8) else _quarter(pal[b], 0x2a55)
                assert img[20 + r, 10 + c] == want, (r, c, b)
        assert img[19, 10] == 0x2a55 and img[20, 14] == 0x2a55


def test_draw_sprite_negctl_1_red(rlib):
    with _state(rlib) as st:
        _sprite(st)
        _underlay(rlib, st, 103)
        with _Neg(rlib, 1):
            _g(st, "Surface_drawSprite", [SPR, ROOT, 10, 20, _alpha(0.25), 0, 0])
        img = _root(st)
        assert img[20, 10] != _quarter(_w555(*PAL[1]), 0x2a55)


def test_draw_sprite_other_alpha_is_unsupported(rlib):
    with _state(rlib) as st:
        _sprite(st)
        _g(st, "Surface_drawSprite", [SPR, ROOT, 0, 0, _alpha(0.5), 0, 0])
        assert "an alpha outside drawAlpha's quarter band" in _log(st)
        assert not _root(st).any()


def test_the_reference_dimming_is_the_555_member():
    """U-1271: the dimming of cov_r_s2_screens' reports is draw16_1002ed60's 555 arithmetic, measured on the references (the
    radial_hover frame quarter-blended with black against rep_comments over the toolbar rows, where the course does not move)"""
    from PIL import Image
    d = GX.REFS / SCENARIO / "det_b"
    if not (d / "radial_hover.png").exists():
        pytest.skip("no pinned reference")

    def w555(n):
        a = np.asarray(Image.open(d / f"{n}.png").convert("RGB")).astype(np.uint32) >> 3
        return (a[..., 0] << 10) | (a[..., 1] << 5) | a[..., 2]
    b, r = w555("radial_hover")[470:], w555("rep_comments")[470:]

    def blend(dw, m1, m2):
        D = dw << 4
        e = ((((D & m1) >> 1) + (D & m1)) >> 1)
        f = ((((D & m2) >> 1) + (D & m2)) >> 1)
        return (((e & m1) + (f & m2)) >> 4) & 0xffff
    assert (blend(b, 0x7c1f0, 0x3e00) == r).mean() > 0.99
    assert (blend(b, 0xf81f0, 0x7e00) == r).mean() < 0.3


# line16 (c3bc.cpp Line16_re) with hline16 / vline16 (c3ay.cpp), written from those transcriptions as a second implementation
def _model_line(img, clip, x1, y1, x2, y2, v):
    L_, T, R, B = clip

    def put(x, y):
        if 0 <= x < W and 0 <= y < H:
            img[y, x] = v
    if x1 == x2:
        if x1 < L_ or x1 >= R or y1 == y2:
            return
        a, b = sorted((y1, y2))
        if a >= B or b < T:
            return
        for y in range(max(a, T), min(b, B - 1) + 1):
            img[y, x1] = v
        return
    if y1 == y2:
        if y1 < T or y1 >= B:
            return
        a, b = sorted((x1, x2))
        if a >= R or b < L_:
            return
        for x in range(max(a, L_), min(b, R - 1) + 1):
            img[y1, x] = v
        return
    if x2 < x1:
        x1, y1, x2, y2 = x2, y2, x1, y1
    dx, dy = x2 - x1, y2 - y1
    if y1 < y2:
        if x1 > R or y1 > B or x2 < L_ or y2 < T:
            return
        if dx >= dy:
            d, ie, ine = 2 * dy - dx, 2 * dy, 2 * (dy - dx)
            while x1 < L_ or y1 < T:
                if d <= 0:
                    d += ie
                else:
                    d += ine
                    y1 += 1
                x1 += 1
            if x1 >= R or y1 >= B:
                return
            put(x1, y1)
            x2 = min(x2, R - 1)
            while x1 < x2:
                if d <= 0:
                    d += ie
                else:
                    y1 += 1
                    if y1 >= B:
                        return
                    d += ine
                x1 += 1
                put(x1, y1)
        else:
            d, ie, ine = 2 * dx - dy, 2 * dx, 2 * (dx - dy)
            while x1 < L_ or y1 < T:
                if d <= 0:
                    d += ie
                else:
                    d += ine
                    x1 += 1
                y1 += 1
            if x1 >= R or y1 >= B:
                return
            put(x1, y1)
            y2 = min(y2, B - 1)
            while y1 < y2:
                if d <= 0:
                    d += ie
                else:
                    x1 += 1
                    if x1 >= R:
                        return
                    d += ine
                y1 += 1
                put(x1, y1)
    else:
        if x1 > R or y1 < T or x2 < L_ or y2 > B:
            return
        dy = abs(dy)
        if dx >= dy:
            d, ie, ine = 2 * dy - dx, 2 * dy, 2 * (dy - dx)
            while x1 < L_ or y1 >= B:
                if d <= 0:
                    d += ie
                else:
                    d += ine
                    y1 -= 1
                x1 += 1
            if x1 >= R or y1 < T:
                return
            put(x1, y1)
            x2 = min(x2, R - 1)
            while x1 < x2:
                if d <= 0:
                    d += ie
                else:
                    y1 -= 1
                    if y1 < T:
                        return
                    d += ine
                x1 += 1
                put(x1, y1)
        else:
            d, ie, ine = 2 * dx - dy, 2 * dx, 2 * (dx - dy)
            while x1 < L_ or y1 >= B:
                if d <= 0:
                    d += ie
                else:
                    d += ine
                    x1 += 1
                y1 -= 1
            if x1 >= R or y1 < T:
                return
            put(x1, y1)
            y2 = max(y2, T)
            while y1 > y2:
                if d <= 0:
                    d += ie
                else:
                    x1 += 1
                    if x1 >= R:
                        return
                    d += ine
                y1 -= 1
                put(x1, y1)


LINES = [(10, 10, 60, 30), (10, 30, 60, 10), (10, 10, 30, 70), (30, 10, 10, 70), (60, 30, 10, 10), (5, 50, 90, 50),
         (40, 5, 40, 80), (40, 40, 40, 40), (90, 5, 100, 120), (-20, -10, 30, 40), (0x6a - 5, 0x60, 0x6a + 40, 0x50)]
BOX = (0x6a, 0x48, 0x256, 0x1c2)


@pytest.mark.parametrize("clipped", [False, True])
def test_widget_fill_rect_equals_the_line_model(rlib, clipped):
    with _state(rlib) as st:
        clip = (0, 0, W, H)
        if clipped:
            _g(st, "Surface_box", [ROOT] + list(BOX))
            clip = (BOX[0], BOX[1], BOX[0] + BOX[2], BOX[1] + BOX[3])
        want = np.zeros((H, W), np.uint16)
        for k, (x1, y1, x2, y2) in enumerate(LINES):
            sx = 0x80 if clipped else 0
            v = 0x100 + k
            _g(st, "Widget_fillRect", [ROOT, x1 + sx, y1 + sx, x2 + sx, y2 + sx, 0x80000000 | v])
            _model_line(want, clip, x1 + sx, y1 + sx, x2 + sx, y2 + sx, v)
        got = _root(st)
        assert (got == want).all(), np.argwhere(got != want)[:5]
        assert want.any()


def test_widget_fill_rect_negctl_5_red(rlib):
    with _state(rlib) as st:
        with _Neg(rlib, 5):
            _g(st, "Widget_fillRect", [ROOT, 10, 10, 60, 30, 0x80007fff])
        want = np.zeros((H, W), np.uint16)
        _model_line(want, (0, 0, W, H), 10, 10, 60, 30, 0x7fff)
        assert (_root(st) != want).any()


def test_surface_box_clips_and_call_bounds_restores(rlib):
    with _state(rlib) as st:
        _g(st, "Surface_box", [ROOT, 100, 100, 50, 50])
        _g(st, "Widget_fillRect", [ROOT, 90, 120, 200, 120, 0x80007fff])
        img = _root(st)
        assert img[120, 100] == 0x7fff and img[120, 149] == 0x7fff and img[120, 99] == 0 and img[120, 150] == 0
        _g(st, "Surface::callBounds", [ROOT])
        _g(st, "Widget_fillRect", [ROOT, 90, 130, 200, 130, 0x80007fff])
        img = _root(st)
        assert img[130, 90] == 0x7fff and img[130, 200] == 0x7fff


def test_surface_box_negctl_7_red(rlib):
    with _state(rlib) as st:
        with _Neg(rlib, 7):
            _g(st, "Surface_box", [ROOT, 100, 100, 50, 50])
        _g(st, "Widget_fillRect", [ROOT, 90, 120, 200, 120, 0x80007fff])
        assert _root(st)[120, 99] == 0x7fff


def test_fill_rect_palette_index_is_unsupported(rlib):
    with _state(rlib) as st:
        _g(st, "Widget_fillRect", [ROOT, 1, 1, 9, 9, 5])
        assert "Widget_fillRect: a palette colour index" in _log(st)


def test_family_has_no_duplicate_body(rlib):
    with _state(rlib) as st:
        st.call("gfx.check", [])
        txt = st.files_read("$gfx/bodies.txt").decode()
        assert "duplicate" not in txt, [ln for ln in txt.splitlines() if ln.startswith("duplicate")]
        for b in ("Surface_drawSprite", "Widget_draw", "drawStringAt", "drawStringAt2", "Widget_draw2", "Surface_box",
                  "Surface::callBounds", "reports.begin", "Widget_fillRect"):
            assert f"body\t{b}\treports" in txt, b


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="judge cov_r_s2_screens checkpoints on the pixels the screens determine")
    ap.add_argument("checkpoints", nargs="*")
    ap.add_argument("--negctl", help="name=mode")
    ap.add_argument("--images", help="directory for port / diff / held PNGs")
    ap.add_argument("--out")
    a = ap.parse_args(argv)
    cps = a.checkpoints or CHECKPOINTS
    lib = L.load()
    held = held_masks(cps, lib)
    neg = None
    if a.negctl:
        n, m = a.negctl.split("=")
        neg = (n, int(m))
    res = judge(cps, lib, neg, held)
    log = res.pop("$log")
    for c in cps:
        print(f"{c}\t{json.dumps(res[c])}", flush=True)
    if a.images:
        from PIL import Image
        d = pathlib.Path(a.images)
        d.mkdir(parents=True, exist_ok=True)
        run = GX.render(SCENARIO, cps, lib, negctl=neg[1] if neg else 0, negctl_name=neg[0] if neg else "gfx.attach")
        for c in cps:
            img = run.frames.get(c)
            if img is None or held[c] is None:
                continue
            ref = GX.reference(SCENARIO, c)
            Image.fromarray(img).save(d / f"{c}.port.png")
            bad = (img != ref).any(axis=2)
            vis = np.zeros(img.shape, np.uint8)
            vis[held[c]] = (60, 60, 60)
            vis[bad & ~held[c]] = (255, 0, 0)
            vis[bad & held[c]] = (0, 0, 160)
            Image.fromarray(vis).save(d / f"{c}.diff.png")
    if a.out:
        pathlib.Path(a.out).write_text(json.dumps({"results": res, "log": log}, indent=1))
    first = [ln for ln in log.splitlines() if ln.startswith("first\t")]
    print("\n".join(first[:40]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
